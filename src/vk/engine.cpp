#include "engine.h"

#include <cassert>
#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <thread>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include <fmt/printf.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include "defines.h"
#include "initializers.h"
#include "utils.h"
#include "pipelinebuilder.h"
#include "vma.h"

#include <chrono>


constexpr bool bUseValidationLayers = true;

Engine *loadedEngine = nullptr;


Engine &Engine::get()
{
	return *loadedEngine;
}

AllocatedBuffer Engine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	assert(allocSize != 0);
	assert(memoryUsage != VMA_MEMORY_USAGE_UNKNOWN);

	// allocate buffer
	const VkBufferCreateInfo bufferInfo {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.size = allocSize,
		.usage = usage,
	};

	const VmaAllocationCreateInfo vmaAllocInfo {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = memoryUsage,
	};

	AllocatedBuffer newBuffer = {};

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

	return newBuffer;
}

void Engine::destroy_buffer(const AllocatedBuffer &buffer)
{
	vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void Engine::init()
{
	// only one engine initialization is allowed with the application.
	assert(loadedEngine == nullptr);
	loadedEngine = this;

	init_sdl();
	init_vulkan();
	init_vma();
	init_swapchain();
	init_commands();
	init_sync_structures();
	init_descriptors();
	init_pipelines();
	init_imgui();
	init_default_data();

	mainCamera.velocity = glm::vec3(0.f);
	mainCamera.position = glm::vec3(0.f, 0.f, 5.f);

	//everything went fine apparently
	_isInitialized = true;
}

void Engine::init_sdl()
{
	// We initialize SDL and create a window with it.
	assert(SDL_Init(SDL_INIT_VIDEO));

	const SDL_WindowFlags window_flags = 0
		| SDL_WINDOW_VULKAN
		| SDL_WINDOW_RESIZABLE;

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

	assert(_window != nullptr);
}

void Engine::init_vulkan()
{
	vkb::InstanceBuilder builder{};

	//make the Vulkan instance, with basic debug features
	const auto inst_ret = builder
		.set_app_name("Example Vulkan Application")
		.request_validation_layers(bUseValidationLayers)
		.require_api_version(1, 3, 0) // We use Vulkan API 1.3, nothing from earlier versions.
		.use_default_debug_messenger()
		.build();

	const vkb::Instance vkb_inst = inst_ret.value();

	//store the instance
	_instance = vkb_inst.instance;
	//store the debug messenger
	_debug_messenger = vkb_inst.debug_messenger;

	assert(_instance != VK_NULL_HANDLE);
	assert(_debug_messenger != VK_NULL_HANDLE);

	assert(SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface));

	const VkPhysicalDeviceVulkan13Features features {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.synchronization2 = true,
		.dynamicRendering = true,
	};

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	const vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)  // We run on Vulkan 1.3+
		.set_required_features_13(features)
		.set_surface(_surface)
		.select()
		.value();

	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		.bufferDeviceAddress = VK_TRUE,
	};

	//create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	const vkb::Device vkbDevice = deviceBuilder
		.add_pNext(&bufferDeviceAddressFeatures)
		.build()
		.value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	assert(_surface != VK_NULL_HANDLE);
	assert(_device != VK_NULL_HANDLE);
	assert(_graphicsQueue != VK_NULL_HANDLE);
}

void Engine::init_vma()
{
	VkImage img;

	// initialize the memory allocator
	const VmaAllocatorCreateInfo allocatorInfo {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = _chosenGPU,
		.device = _device,
		.instance = _instance,
		.vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0),
	};
	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_allocator));

	_mainDeletionQueue.push_function([this]() {
		vmaDestroyAllocator(_allocator);
	});
}

void Engine::init_swapchain()
{
	create_swapchain(_windowExtent.width, _windowExtent.height);

	//draw image size will match the window
	// [NOTE] Previously was only windowExtent, but changed to this model in case the size differs.
	const VkExtent3D drawImageExtent {
		std::min(_swapchainExtent.width, _windowExtent.width),
		std::min(_swapchainExtent.height, _windowExtent.height),
		1,
	};

	// Color IMG
	//hardcoding the draw format to 32 bit float
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	constexpr VkImageUsageFlags drawImageUsages = 0
		 | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		 | VK_IMAGE_USAGE_TRANSFER_DST_BIT
		 | VK_IMAGE_USAGE_STORAGE_BIT
		 | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	const VmaAllocationCreateInfo rimg_allocinfo {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};

	//allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr));

	//build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

	//add to deletion queues
	_mainDeletionQueue.push_function([this]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
	});

	// Depth IMG
	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	const VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	const VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

	//allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr));

	//build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

	_mainDeletionQueue.push_function([this]() {
		vkDestroyImageView(_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
	});
}

void Engine::init_commands()
{
	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	const VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		assert(_frames[i]._commandPool != VK_NULL_HANDLE);
		assert(_frames[i]._mainCommandBuffer != VK_NULL_HANDLE);
	}

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

	// allocate the command buffer for immediate submits
	const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

	_mainDeletionQueue.push_function([this]() {
		vkDestroyCommandPool(_device, _immCommandPool, nullptr);
	});
}

void Engine::init_sync_structures()
{
	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	const VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		assert(_frames[i]._renderFence != VK_NULL_HANDLE);
		assert(_frames[i]._swapchainSemaphore != VK_NULL_HANDLE);
		assert(_frames[i]._renderSemaphore != VK_NULL_HANDLE);
	}

	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
	_mainDeletionQueue.push_function([this]() {
		vkDestroyFence(_device, _immFence, nullptr);
	});
}

void Engine::init_descriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	constexpr DescriptorAllocatorGrowable::PoolSizeRatio sizes[] = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
	};

	globalDescriptorAllocator.init(_device, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder{};
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		_drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	//allocate a descriptor set for our draw image
	_drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

	DescriptorWriter writer{};
	writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(_device,_drawImageDescriptors);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		constexpr DescriptorAllocatorGrowable::PoolSizeRatio frame_sizes[] = {
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable();
		_frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);

		_mainDeletionQueue.push_function([this, i]() {
			_frames[i]._frameDescriptors.destroy_pools(_device);
		});
	}

	{
		DescriptorLayoutBuilder builder{};
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		_gpuSceneDataDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder{};
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_singleImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}


	//make sure both the descriptor allocator and the new layout get cleaned up properly
	_mainDeletionQueue.push_function([this]() {
		globalDescriptorAllocator.destroy_pools(_device);

		vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorLayout, nullptr);
	});

	metalRoughMaterial.build_pipelines(*this);
}

void Engine::init_pipelines()
{
	init_background_pipelines();

	init_mesh_pipeline();
}

void Engine::init_mesh_pipeline()
{
	/*VkShaderModule triangleFragShader;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/colored_triangle.frag.spv", _device, triangleFragShader)) {
		fmt::print("Error when building the triangle fragment shader module");
	} else {
		fmt::print("Triangle fragment shader succesfully loaded\n");
	}

	VkShaderModule triangleVertexShader;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/colored_triangle_mesh.vert.spv", _device, triangleVertexShader)) {
		fmt::print("Error when building the triangle vertex shader module\n");
	} else {
		fmt::print("Triangle vertex shader succesfully loaded\n");
	}

	const VkPushConstantRange bufferRange {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUDrawPushConstants),
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;*/

	VkShaderModule triangleFragShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/tex_image.frag.spv", _device, triangleFragShader)) {
		fmt::print("Error when building the fragment shader\n");
	} else {
		fmt::print("Triangle fragment shader succesfully loaded\n");
	}

	VkShaderModule triangleVertexShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/colored_triangle_mesh.vert.spv", _device, triangleVertexShader)) {
		fmt::print("Error when building the vertex shader\n");
	} else {
		fmt::print("Triangle vertex shader succesfully loaded\n");
	}

	/*const VkPushConstantRange bufferRange {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUDrawPushConstants),
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
	pipeline_layout_info.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

	PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	//pipelineBuilder.disable_blending();
	pipelineBuilder.enable_blending_additive();

	//pipelineBuilder.disable_depthtest();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(_depthImage.imageFormat);

	//finally build the pipeline
	_meshPipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
	});*/

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
	pipeline_layout_info.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

	//< mesh_shader

	PipelineBuilder pipelineBuilder{};

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	//pipelineBuilder.disable_blending();
	pipelineBuilder.disable_blending();
	//pipelineBuilder.enable_blending_additive();
	//no depth testing
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(_depthImage.imageFormat);

	//finally build the pipeline
	_meshPipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
	});
}

void Engine::init_background_pipelines()
{
	const VkPushConstantRange pushConstant {
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0,
		.size = sizeof(ComputePushConstants),
	};

	const VkPipelineLayoutCreateInfo computeLayout {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.setLayoutCount = 1,
		.pSetLayouts = &_drawImageDescriptorLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstant,
	};

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

	//layout code
	VkShaderModule computeDrawShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/gradient.comp.spv", _device, computeDrawShader)) {
		fmt::print("Error when building the compute shader \n");
	}

	const VkPipelineShaderStageCreateInfo stageinfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = computeDrawShader,
		.pName = "main",
	};

	const VkComputePipelineCreateInfo computePipelineCreateInfo {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.stage = stageinfo,
		.layout = _gradientPipelineLayout,
	};

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

	vkDestroyShaderModule(_device, computeDrawShader, nullptr);

	_mainDeletionQueue.push_function([this]() {
		vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _gradientPipeline, nullptr);
	});
}

void Engine::init_imgui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	constexpr VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
	};

	const VkDescriptorPoolCreateInfo pool_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes)),
		.pPoolSizes = pool_sizes,
	};

	VkDescriptorPool imguiPool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL3_InitForVulkan(_window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info {
		.Instance = _instance,
		.PhysicalDevice = _chosenGPU,
		.Device = _device,
		.Queue = _graphicsQueue,
		.DescriptorPool = imguiPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.UseDynamicRendering = true,

		//dynamic rendering parameters for imgui to use
		.PipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &_swapchainImageFormat,
		},
	};

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	_mainDeletionQueue.push_function([this, imguiPool]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
	});
}

void Engine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function)
{
	VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	VkCommandBuffer cmd = _immCommandBuffer;

	const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	const VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void Engine::cleanup()
{
	if (_isInitialized) {
		// We need to wait fort he GPU to finish until...
		VK_CHECK(vkDeviceWaitIdle(_device));

		// we can destroy the command pools.
		// It may crash the app otherwise.
		/*for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
		}*/
		for (int i = 0; i < FRAME_OVERLAP; i++) {
			//already written from before
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

			_frames[i]._deletionQueue.flush();

			_frames[i]._commandPool = VK_NULL_HANDLE;
			_frames[i]._renderFence = VK_NULL_HANDLE;
			_frames[i]._renderSemaphore = VK_NULL_HANDLE;
			_frames[i]._swapchainSemaphore = VK_NULL_HANDLE;
		}

		/*for (auto &mesh : testMeshes) {
			destroy_buffer(mesh->meshBuffers.indexBuffer);
			destroy_buffer(mesh->meshBuffers.vertexBuffer);
		}*/

		//flush the global deletion queue
		_mainDeletionQueue.flush();

#ifdef VMA_USE_DEBUG_LOG
		// Right here because all VMA (de)allocs must all have been performed
		// just when we finish flushing the main queue.
		assert(allocationCounter == 0 && "Memory leak detected!");
#endif

		//destroy swapchain-associated resources
		destroy_swapchain();

		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);
		SDL_DestroyWindow(_window);

		_device = VK_NULL_HANDLE;
		_instance = VK_NULL_HANDLE;
		_surface = VK_NULL_HANDLE;
		_debug_messenger = VK_NULL_HANDLE;
		_window = nullptr;
	}

	// clear engine pointer
	loadedEngine = nullptr;
}

void Engine::run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit) {
		// Handle events on queue
		while (SDL_PollEvent(&e) != 0) {
			// close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_EVENT_QUIT) {
				bQuit = true;
			}

			if (e.type == SDL_EVENT_WINDOW_MINIMIZED) {
				stop_rendering = true;
			}
			if (e.type == SDL_EVENT_WINDOW_RESTORED) {
				stop_rendering = false;
			}

			mainCamera.processSDLEvent(e);
			ImGui_ImplSDL3_ProcessEvent(&e);
		}

		//do not draw if we are minimized
		if (stop_rendering) {
			//throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		if (ImGui::Begin("background")) {
			ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);

			/*ComputeEffect &selected = backgroundEffects[currentBackgroundEffect];

			ImGui::Text("Selected effect: %s", selected.name);

			ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

			ImGui::InputFloat4("data1", reinterpret_cast<float *>(&selected.data.data1));
			ImGui::InputFloat4("data2", reinterpret_cast<float *>(&selected.data.data2));
			ImGui::InputFloat4("data3", reinterpret_cast<float *>(&selected.data.data3));
			ImGui::InputFloat4("data4", reinterpret_cast<float *>(&selected.data.data4));*/

			ImGui::End();
		}

		ImGui::Render();

		draw();

		if (resize_requested) {
			resize_swapchain();
		}
	}
}

void Engine::draw()
{
	update_scene();

	auto &_currFrame = get_current_frame();

	assert(_currFrame._renderFence != VK_NULL_HANDLE);
	// wait until the gpu has finished rendering the last frame. Timeout of 1
	// second
	VK_CHECK(vkWaitForFences(_device, 1, &_currFrame._renderFence, true, 1000000000));
	_currFrame._deletionQueue.flush();
	_currFrame._frameDescriptors.clear_pools(_device);

	assert(_device != VK_NULL_HANDLE);
	assert(_swapchain != VK_NULL_HANDLE);
	assert(_currFrame._swapchainSemaphore != VK_NULL_HANDLE);
	//request image from the swapchain

	uint32_t swapchainImageIndex = -1;
	const VkResult acquireResult = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _currFrame._swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}
	// [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

	VK_CHECK(vkResetFences(_device, 1, &_currFrame._renderFence));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = _currFrame._mainCommandBuffer;
	assert(cmd != VK_NULL_HANDLE);

	assert(cmd != VK_NULL_HANDLE);
	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale;
	_drawExtent.width= std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	draw_pc(cmd);
	draw_background(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	draw_geometry(cmd);

	//transtion the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

	//draw imgui into the swapchain image
	draw_imgui(cmd,  _swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can show it on the screen
	vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue.
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, _currFrame._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, _currFrame._renderSemaphore);

	const VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _currFrame._renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as its necessary that drawing commands have finished before the image is displayed to the user
	const VkPresentInfoKHR presentInfo {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &_currFrame._renderSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &_swapchain,
		.pImageIndices = &swapchainImageIndex,
	};

	const VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
	}
	// [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

	//increase the number of frames drawn
	_frameNumber++;
}

void Engine::draw_background(VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	/*/make a clear-color from frame number. This will flash with a 120 frame period.
	const float flash = std::abs(std::sin(_frameNumber / 120.f));
	const VkClearColorValue clearValue { { 0.0f, 0.0f, flash, 1.0f } };

	const VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	vkCmdClearColorImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);*/

	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void Engine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	assert(cmd != VK_NULL_HANDLE);
	assert(targetImageView != VK_NULL_HANDLE);

	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void Engine::draw_pc(VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	const ComputePushConstants pc {
		.data1 = glm::vec4(1, 0, 0, 1),
		.data2 = glm::vec4(0, 0, 1, 1),
	};

	vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pc);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

#include <chrono>

// Store the application start time (global or in your initialization code)
const auto startTime = std::chrono::high_resolution_clock::now();

void Engine::draw_geometry(VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	//begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	const VkRenderingInfo renderInfo = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment);

	//set dynamic viewport and scissor
	const VkViewport viewport {
		.x = 0,
		.y = 0,
		.width = static_cast<float>(_drawExtent.width),
		.height = static_cast<float>(_drawExtent.height),
		.minDepth = 0.f,
		.maxDepth = 1.f,
	};

	const VkRect2D scissor {
		.offset = {
			.x = 0,
			.y = 0,
		},
		.extent = {
			.width = _drawExtent.width,
			.height = _drawExtent.height,
		},
	};

	// Custom impl to show it nicely
	// Calculate the elapsed time in seconds
	const auto currentTime = std::chrono::high_resolution_clock::now();
	const float time = std::chrono::duration<float>(currentTime - startTime).count();

	// Define the rotation angle over time
	const float angle = glm::radians(90.0f) * time; // Rotates 90 degrees per second

	// Rotation matrix around the Y-axis
	const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));

	// Translation matrix to move the object back over time
	const glm::mat4 backTranslation = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 3.0f - time));

	// Combine the transformations
	const glm::mat4 worldMatrix = backTranslation * rotation;

	// Calculate the view and projection matrices
	const glm::mat4 view = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.0f, -5.0f));
	glm::mat4 projection = glm::perspective(
		glm::radians(70.0f),
		static_cast<float>(_drawExtent.width) / static_cast<float>(_drawExtent.height),
		0.1f, 10000.0f
	);

	// Invert the Y direction of the projection matrix for OpenGL compatibility
	projection[1][1] *= -1.0f;

	// Push constants with the animated world matrix
	const GPUDrawPushConstants push_constants {
		.worldMatrix = projection * view * worldMatrix,
		.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress,
	};


	/*vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

	//bind a texture
	VkDescriptorSet imageSet = get_current_frame()._frameDescriptors.allocate(_device, _singleImageDescriptorLayout);
	{
		DescriptorWriter writer;

		writer.write_image(0, _errorCheckerboardImage.imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		writer.update_set(_device, imageSet);
	}

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
	vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, testMeshes[2]->surfaces[0].count, 1, testMeshes[2]->surfaces[0].startIndex, 0, 0);

	vkCmdEndRendering(cmd);*/


	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	auto &currFrame = get_current_frame();

	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	currFrame._deletionQueue.push_function([=, this]() {
		destroy_buffer(gpuSceneDataBuffer);
	});

	//write the buffer
	GPUSceneData *sceneUniformData = reinterpret_cast<GPUSceneData *>(getMappedData(gpuSceneDataBuffer.allocation));
	*sceneUniformData = sceneData;

	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = currFrame._frameDescriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, globalDescriptor);


	for (const RenderObject &draw : mainDrawContext.OpaqueSurfaces) {
		assert(draw.material);
		assert(draw.material->materialSet != VK_NULL_HANDLE);
		assert(draw.material->passType != MaterialPass::Invalid);
		assert(draw.material->pipeline != nullptr);
		assert(draw.material->pipeline->pipeline != VK_NULL_HANDLE);
		assert(draw.material->pipeline->layout != VK_NULL_HANDLE);
		assert(draw.vertexBufferAddress != 0);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 1, 1, &draw.material->materialSet, 0, nullptr);

		vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		const GPUDrawPushConstants pushConstants {
			.worldMatrix = draw.transform,
			.vertexBuffer = draw.vertexBufferAddress,
		};
		vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0); // [DEFECT]
	}

	vkCmdEndRendering(cmd);
}

GPUMeshBuffers Engine::uploadMesh(const std::span<const uint32_t> &indices, const std::span<const Vertex> &vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface {
		//create index buffer
		.indexBuffer = create_buffer(
			indexBufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY
		),
		//create vertex buffer
		.vertexBuffer = create_buffer(
			vertexBufferSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY
		),
	};

	//find the adress of the vertex buffer
	const VkBufferDeviceAddressInfo deviceAdressInfo {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = newSurface.vertexBuffer.buffer
	};
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

	AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void *data = getMappedData(staging.allocation);
	assert(data != nullptr);

	// copy vertex buffer
	std::memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer
	std::memcpy(reinterpret_cast<char *>(data) + vertexBufferSize, indices.data(), indexBufferSize);

	immediate_submit([&](VkCommandBuffer cmd) {
		const VkBufferCopy vertexCopy {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = vertexBufferSize,
		};

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		const VkBufferCopy indexCopy {
			.srcOffset = vertexBufferSize,
			.dstOffset = 0,
			.size = indexBufferSize,
		};

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
	});

	destroy_buffer(staging);

	return newSurface;
}

void Engine::init_default_data()
{
	testMeshes = loadGltfMeshes(*this, ASSETS_DIR "/basicmesh.glb").value();
	assert(testMeshes.size() == 3);
	//delete the meshes data on engine shutdown
	_mainDeletionQueue.push_function([&](){
		for (const auto &v : testMeshes) {
			destroy_buffer(v->meshBuffers.indexBuffer);
			destroy_buffer(v->meshBuffers.vertexBuffer);
		}
	});

	//3 default textures, white, grey, black. 1 pixel each
	const uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = create_image(reinterpret_cast<const void *>(&white), VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	const uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = create_image(reinterpret_cast<const void *>(&grey), VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_blackImage = create_image(reinterpret_cast<const void *>(&black), VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16*16> pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampl = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
	};

	VK_CHECK(vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest));

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	VK_CHECK(vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear));

	_mainDeletionQueue.push_function([this](){
		vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
		vkDestroySampler(_device, _defaultSamplerLinear, nullptr);

		destroy_image(_whiteImage);
		destroy_image(_greyImage);
		destroy_image(_blackImage);
		destroy_image(_errorCheckerboardImage);
	});

	GLTFMetallic_Roughness::MaterialResources materialResources {
		//default the material textures
		.colorImage = _whiteImage,
		.colorSampler = _defaultSamplerLinear,
		.metalRoughImage = _whiteImage,
		.metalRoughSampler = _defaultSamplerLinear,
	};

	//set the uniform buffer for the material data
	AllocatedBuffer materialConstants = create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//write the buffer
	GLTFMetallic_Roughness::MaterialConstants *sceneUniformData = reinterpret_cast<GLTFMetallic_Roughness::MaterialConstants *>(getMappedData(materialConstants.allocation));
	assert(sceneUniformData != nullptr);
	sceneUniformData->colorFactors = glm::vec4{1.f, 1.f, 1.f, 1.f};
	sceneUniformData->metal_rough_factors = glm::vec4{1,0.5,0,0};

	_mainDeletionQueue.push_function([=, this]() {
		destroy_buffer(materialConstants);
	});

	materialResources.dataBuffer = materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	defaultData = metalRoughMaterial.write_material(_device, MaterialPass::MainColor, materialResources, globalDescriptorAllocator);

	for (const auto &m : testMeshes) {
		std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
		newNode->mesh = m;

		newNode->localTransform = glm::mat4{1.f};
		newNode->worldTransform = glm::mat4{1.f};

		for (auto &s : newNode->mesh->surfaces) {
			s.material = std::make_shared<GLTFMaterial>(defaultData);
		}

		loadedNodes[m->name] = std::move(newNode);
	}
}

void Engine::create_swapchain(uint32_t w, uint32_t h)
{
	vkb::SwapchainBuilder swapchainBuilder {
		_chosenGPU,
		_device,
		_surface,
	};

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR {
			.format = _swapchainImageFormat,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		})
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(w, h)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
	_swapchainExtent = vkbSwapchain.extent;

	assert(_swapchain != VK_NULL_HANDLE);
	assert(_swapchainImages.size() > 1);
	assert(_swapchainImages.size() == _swapchainImageViews.size());
}

void Engine::destroy_swapchain()
{
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (auto &iv : _swapchainImageViews) {
		vkDestroyImageView(_device, iv, nullptr);
	}
}

void Engine::resize_swapchain()
{
	VK_CHECK(vkDeviceWaitIdle(_device));

	destroy_swapchain();

	int w = -1, h = -1;
	SDL_GetWindowSize(_window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	create_swapchain(_windowExtent.width, _windowExtent.height);

	resize_requested = false;
}

AllocatedImage Engine::create_image(const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped)
{
	assert(format != VK_FORMAT_MAX_ENUM);
	assert(usage != VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM);

	AllocatedImage newImage {
		.imageExtent = size,
		.imageFormat = format,
	};

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	const VmaAllocationCreateInfo allocinfo = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	const VkImageAspectFlags aspectFlag = format != VK_FORMAT_D32_SFLOAT
		? VK_IMAGE_ASPECT_COLOR_BIT
		: VK_IMAGE_ASPECT_DEPTH_BIT;

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage Engine::create_image(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped)
{
	assert(data != nullptr);
	assert(format != VK_FORMAT_MAX_ENUM);
	assert(usage != VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM);

	const size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadbuffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	std::memcpy(uploadbuffer.info.pMappedData, data, data_size);

	AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	immediate_submit([&](VkCommandBuffer cmd) {
		vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		const VkBufferImageCopy copyRegion = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.imageExtent = size,
		};

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});

	destroy_buffer(uploadbuffer);

	return new_image;
}

void Engine::destroy_image(const AllocatedImage &img)
{
	vkDestroyImageView(_device, img.imageView, nullptr);
	vmaDestroyImage(_allocator, img.image, img.allocation);
}

void Engine::update_scene()
{
	mainDrawContext.OpaqueSurfaces.clear();

	for (int x = -3; x < 3; x++) {
		const glm::mat4 scale = glm::scale(glm::mat4{1.f}, glm::vec3{0.2f});
		const glm::mat4 translation =  glm::translate(glm::mat4{1.f}, glm::vec3{x, 1.f, 0.f});

		loadedNodes["Cube"]->draw(translation * scale, mainDrawContext);
	}

	loadedNodes["Suzanne"]->draw(glm::mat4{1.f}, mainDrawContext);

	mainCamera.update();

	const glm::mat4 view = mainCamera.getViewMatrix();

	// camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	projection[1][1] *= -1;

	sceneData.view = view;
	sceneData.proj = projection;
	sceneData.viewproj = projection * view;

	sceneData.view = glm::translate(glm::mat4{1.f}, glm::vec3{0.f ,0.f, -5.f});
	// camera projection
	sceneData.proj = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	sceneData.proj[1][1] *= -1;
	sceneData.viewproj = sceneData.proj * sceneData.view;

	//some default lighting parameters
	sceneData.ambientColor = glm::vec4(.1f);
	sceneData.sunlightColor = glm::vec4(1.f);
	sceneData.sunlightDirection = glm::vec4(0,1,0.5,1.f);
}
