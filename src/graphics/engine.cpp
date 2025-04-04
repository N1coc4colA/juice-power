#include "engine.h"

#include <cassert>
#include <cstring>
#include <chrono>
#include <iostream>
#include <thread>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include <fmt/printf.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include "failure.h"

#include "defines.h"
#include "initializers.h"
#include "utils.h"
#include "pipelinebuilder.h"
#include "vma.h"


#define LOGFN() {std::cout << __func__ << std::endl;}


constexpr bool bUseValidationLayers = true;


namespace Graphics
{

Engine *loadedEngine = nullptr;



Engine &Engine::get()
{
	return *loadedEngine;
}

AllocatedBuffer Engine::createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage)
{
	LOGFN();

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

	// allocate the buffer
	AllocatedBuffer newBuffer {};
	VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));
	if (newBuffer.buffer == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkBufferAllocation);
	}

	return newBuffer;
}

void Engine::destroyBuffer(const AllocatedBuffer &buffer)
{
	LOGFN();

	vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void Engine::init()
{
	LOGFN();

	// only one Engine initialization is allowed with the application.
	assert(loadedEngine == nullptr);
	loadedEngine = this;

	initSDL();
	initVulkan();
	initVMA();
	initSwapchain();
	initCommands();
	initSyncStructures();
	initDescriptors();
	initPipelines();
	initDefaultData();
	initImgui();

	//everything went fine apparently
	isInitialized = true;
}

void Engine::initSDL()
{
	LOGFN();

	// We initialize SDL and create a window with it.
	const bool sdlInitd = SDL_Init(SDL_INIT_VIDEO);
	if (!sdlInitd) {
		throw new Failure(FailureType::SDLInitialisation);
	}

	constexpr SDL_WindowFlags window_flags = 0
		| SDL_WINDOW_VULKAN
		| SDL_WINDOW_RESIZABLE;

	window = SDL_CreateWindow(
		"Vulkan Engine",
		windowExtent.width,
		windowExtent.height,
		window_flags
	);

	if (window == nullptr) {
		throw new Failure(FailureType::SDLWindowCreation);
	}
}

void Engine::initVulkan()
{
	LOGFN();

	//make the Vulkan instance, with basic debug features
	const auto instRet = vkb::InstanceBuilder()
		.set_app_name("Example Vulkan Application")
		.request_validation_layers(bUseValidationLayers)
		.require_api_version(1, 3, 0) // We use Vulkan API 1.3, nothing from earlier versions.
		.use_default_debug_messenger()
		.build();

	const vkb::Instance vkb_inst = instRet.value();

	//store the instance
	instance = vkb_inst.instance;
	if (instance == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkInstanceCreation);
	}
	//store the debug messenger
	debugMessenger = vkb_inst.debug_messenger;
	if (debugMessenger == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkDebugMessengerCreation);
	}

	const bool surfaceCreated = SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);
	if (!surfaceCreated) {
		throw new Failure(FailureType::VkSurfaceCreation1);
	}

	constexpr VkPhysicalDeviceVulkan13Features features13 {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.synchronization2 = true,
		.dynamicRendering = true,
	};

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.3
	const vkb::PhysicalDevice physicalDevice = vkb::PhysicalDeviceSelector(vkb_inst)
		.set_minimum_version(1, 3)  // We run on Vulkan 1.3+
		.set_required_features_13(features13)
		.set_surface(surface)
		.select()
		.value();

	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
		.bufferDeviceAddress = VK_TRUE,
	};

	//create the final Vulkan device
	const vkb::Device vkbDevice = vkb::DeviceBuilder(physicalDevice)
		.add_pNext(&bufferDeviceAddressFeatures)
		.build()
		.value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	device = vkbDevice.device;
	chosenGPU = physicalDevice.physical_device;

	graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	if (surface == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkSurfaceCreation2);
	}
	if (device == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkDeviceCreation);
	}
	if (graphicsQueue == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkQueueCreation);
	}
}

void Engine::initVMA()
{
	LOGFN();

	// initialize the memory allocator
	const VmaAllocatorCreateInfo allocatorInfo {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = chosenGPU,
		.device = device,
		.instance = instance,
		.vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0),
	};

	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator));
	if (allocator == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VMAInitialisation);
	}

	mainDeletionQueue.push_function([this]() {
		vmaDestroyAllocator(allocator);
	});
}

void Engine::initSwapchain()
{
	LOGFN();

	createSwapchain(windowExtent.width, windowExtent.height);

	//draw image size will match the window
	// [NOTE] Previously was only windowExtent, but changed to this model in case the size differs.
	const VkExtent3D drawImageExtent {
		std::min(swapchainExtent.width, windowExtent.width),
		std::min(swapchainExtent.height, windowExtent.height),
		1,
	};

	// Color IMG
	//hardcoding the draw format to 32 bit float
	drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	drawImage.imageExtent = drawImageExtent;

	constexpr VkImageUsageFlags drawImageUsages = 0
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	const VmaAllocationCreateInfo rimg_allocinfo {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};

	//allocate and create the image
	VK_CHECK(vmaCreateImage(allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr));
	if (drawImage.image == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VMAImageCreation, "Drawing");
	}

	//build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &drawImage.imageView));
	if (drawImage.imageView == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VMAImageViewCreation, "Drawing");
	}

	//add to deletion queues
	mainDeletionQueue.push_function([this]() {
		vkDestroyImageView(device, drawImage.imageView, nullptr);
		vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
	});

	// Depth IMG
	depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	depthImage.imageExtent = drawImageExtent;
	constexpr VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	const VkImageCreateInfo dimg_info = vkinit::image_create_info(depthImage.imageFormat, depthImageUsages, drawImageExtent);

	//allocate and create the image
	VK_CHECK(vmaCreateImage(allocator, &dimg_info, &rimg_allocinfo, &depthImage.image, &depthImage.allocation, nullptr));
	if (depthImage.image == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VMAImageCreation, "Depth");
	}

	//build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(device, &dview_info, nullptr, &depthImage.imageView));
	if (depthImage.imageView == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VMAImageViewCreation, "Depth");
	}

	mainDeletionQueue.push_function([this]() {
		vkDestroyImageView(device, depthImage.imageView, nullptr);
		vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
	});
}

void Engine::initCommands()
{
	LOGFN();

	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	const VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool));
		if (frames[i].commandPool == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkCommandPoolCreation, "Frame");
		}

		// allocate the default command buffer that we will use for rendering
		const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frames[i].commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].mainCommandBuffer));
		if (frames[i].mainCommandBuffer == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkCommandBufferCreation, "Frame");
		}
	}

	VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immCommandPool));
	if (immCommandPool == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkCommandPoolCreation, "Immediate");
	}

	// allocate the command buffer for immediate submits
	const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &immCommandBuffer));
	if (immCommandBuffer == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkCommandBufferCreation, "Immediate");
	}

	mainDeletionQueue.push_function([this]() {
		vkDestroyCommandPool(device, immCommandPool, nullptr);
	});
}

void Engine::initSyncStructures()
{
	LOGFN();

	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	const VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));
		if (frames[i].renderFence == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkFenceCreation, "Frame");
		}

		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
		if (frames[i].swapchainSemaphore == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkSwapchainCreation, "Frame");
		}
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore));
		if (frames[i].renderSemaphore == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkSemaphoreCreation, "Frame");
		}
	}

	VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence));
	if (immFence == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkFenceCreation, "Immediate");
	}

	mainDeletionQueue.push_function([this]() {
		vkDestroyFence(device, immFence, nullptr);
	});
}

void Engine::initDescriptors()
{
	LOGFN();

	//create a descriptor pool that will hold 10 sets with 1 image each
	constexpr DescriptorAllocatorGrowable::PoolSizeRatio sizes[] {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
	};

	globalDescriptorAllocator.init(device, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder {};
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
		if (drawImageDescriptorLayout == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkDescriptorCreation, "Draw");
		}
	}

	//allocate a descriptor set for our draw image
	drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

	DescriptorWriter writer {};
	writer.writeImage(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.updateSet(device, drawImageDescriptors);
	if (drawImageDescriptors == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkDescriptorUpdate, "Draw");
	}

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		constexpr DescriptorAllocatorGrowable::PoolSizeRatio frame_sizes[] = {
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		};

		frames[i].frameDescriptors = DescriptorAllocatorGrowable();
		frames[i].frameDescriptors.init(device, 1000, frame_sizes);

		mainDeletionQueue.push_function([this, i]() {
			frames[i].frameDescriptors.destroyPools(device);
		});
	}

	{
		DescriptorLayoutBuilder builder {};
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		gpuSceneDataDescriptorLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		if (gpuSceneDataDescriptorLayout == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkDescriptorLayoutCreation, "GPU");
		}
	}

	{
		DescriptorLayoutBuilder builder {};
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		singleImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
		if (singleImageDescriptorLayout == VK_NULL_HANDLE) {
			throw new Failure(FailureType::VkDescriptorLayoutCreation, "Single");
		}
	}

	//make sure both the descriptor allocator and the new layout get cleaned up properly
	mainDeletionQueue.push_function([this]() {
		globalDescriptorAllocator.destroyPools(device);

		vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, gpuSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, singleImageDescriptorLayout, nullptr);
	});
}

void Engine::initPipelines()
{
	LOGFN();

	initBackgroundPipelines();
	initMeshPipeline();
}

void Engine::initMeshPipeline()
{
	LOGFN();

	VkShaderModule triangleFragShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/tex_image.frag.spv", device, triangleFragShader)) {
		throw new Failure(FailureType::FragmentShader, "Triangle");
	}

	VkShaderModule triangleVertexShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/colored_triangle_mesh.vert.spv", device, triangleVertexShader)) {
		throw new Failure(FailureType::VertexShader, "Triangle");
	}

	const VkPushConstantRange bufferRange {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUDrawPushConstants),
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = &singleImageDescriptorLayout;
	pipeline_layout_info.setLayoutCount = 1;

	VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &meshPipelineLayout));
	if (meshPipelineLayout == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkPipelineLayoutCreation);
	}

	PipelineBuilder pipelineBuilder {};
	//use the triangle layout we created
	pipelineBuilder.pipelineLayout = meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.setShaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	//pipelineBuilder.setCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	//no multisampling
	pipelineBuilder.setMultisamplingNone();
	//no blending
	//pipelineBuilder.disable_blending();
	pipelineBuilder.enableBlendingAdditive();
	//pipelineBuilder.disable_depthtest();
	pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	//connect the image format we will draw into, from draw image
	pipelineBuilder.setColorAttachmentFormat(drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(depthImage.imageFormat);

	//finally build the pipeline
	meshPipeline = pipelineBuilder.buildPipeline(device);
	if (meshPipeline == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkPipelineCreation);
	}

	//clean structures
	vkDestroyShaderModule(device, triangleFragShader, nullptr);
	vkDestroyShaderModule(device, triangleVertexShader, nullptr);

	mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);
		vkDestroyPipeline(device, meshPipeline, nullptr);
	});
}

void Engine::initBackgroundPipelines()
{
	LOGFN();

	constexpr VkPushConstantRange pushConstant {
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0,
		.size = sizeof(ComputePushConstants),
	};

	const VkPipelineLayoutCreateInfo computeLayout {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.setLayoutCount = 1,
		.pSetLayouts = &drawImageDescriptorLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstant,
	};

	VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &gradientPipelineLayout));
	if (gradientPipelineLayout == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkPipelineLayoutCreation, "Gradient");
	}

	//layout code
	VkShaderModule computeDrawShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/gradient.comp.spv", device, computeDrawShader)) {
		throw new Failure(FailureType::ComputeShader);
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
		.layout = gradientPipelineLayout,
	};

	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientPipeline));
	if (gradientPipeline == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkPipelineCreation, "Gradient");
	}

	vkDestroyShaderModule(device, computeDrawShader, nullptr);

	mainDeletionQueue.push_function([this]() {
		vkDestroyPipelineLayout(device, gradientPipelineLayout, nullptr);
		vkDestroyPipeline(device, gradientPipeline, nullptr);
	});
}

void Engine::initImgui()
{
	LOGFN();

	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	constexpr VkDescriptorPoolSize pool_sizes[] {
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
	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));
	if (imguiPool == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkDescriptorPoolCreation);
	}

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	const auto igCtx = ImGui::CreateContext();
	if (igCtx == nullptr) {
		throw new Failure(FailureType::ImguiContext);
	}

	// this initializes imgui for SDL
	const bool igS3VkInited = ImGui_ImplSDL3_InitForVulkan(window);
	if (!igS3VkInited) {
		throw new Failure(FailureType::ImguiInitialisation);
	}

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info {
		.Instance = instance,
		.PhysicalDevice = chosenGPU,
		.Device = device,
		.Queue = graphicsQueue,
		.DescriptorPool = imguiPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.UseDynamicRendering = true,

		//dynamic rendering parameters for imgui to use
		.PipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapchainImageFormat,
		},
	};

	const bool igVkInited = ImGui_ImplVulkan_Init(&init_info);
	if (!igVkInited) {
		throw new Failure(FailureType::ImguiVkInitialisation);
	}

	const bool igFtInited = ImGui_ImplVulkan_CreateFontsTexture();
	if (!igFtInited) {
		throw new Failure(FailureType::ImguiFontsInitialisation);
	}

	// add the destroy the imgui created structures
	mainDeletionQueue.push_function([this, imguiPool]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(device, imguiPool, nullptr);
	});
}

void Engine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function)
{
	LOGFN();

	assert(immFence);
	assert(immCommandBuffer);

	VK_CHECK(vkResetFences(device, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

	const VkCommandBuffer cmd = immCommandBuffer;

	const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	const VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	const VkSubmitInfo2 submit = vkinit::submit_info(cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, immFence));

	VK_CHECK(vkWaitForFences(device, 1, &immFence, true, 9999999999));
}

void Engine::cleanup()
{
	LOGFN();

	if (isInitialized) {
		// We need to wait fort he GPU to finish until...
		VK_CHECK(vkDeviceWaitIdle(device));

		// we can destroy the command pools.
		// It may crash the app otherwise.
		for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(device, frames[i].commandPool, nullptr);
		}
		for (int i = 0; i < FRAME_OVERLAP; i++) {
			//destroy sync objects
			vkDestroyFence(device, frames[i].renderFence, nullptr);
			vkDestroySemaphore(device, frames[i].renderSemaphore, nullptr);
			vkDestroySemaphore(device, frames[i].swapchainSemaphore, nullptr);

			frames[i].deletionQueue.flush();
		}

		//flush the global deletion queue
		mainDeletionQueue.flush();

#ifdef VMA_USE_DEBUG_LOG
		// Right here because all VMA (de)allocs must all have been performed
		// just when we finish flushing the main queue.
		assert(allocationCounter == 0 && "Memory leak detected!");
#endif

		//destroy swapchain-associated resources
		destroySwapchain();

		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkb::destroy_debug_utils_messenger(instance, debugMessenger);
		vkDestroyInstance(instance, nullptr);
		SDL_DestroyWindow(window);

		device = VK_NULL_HANDLE;
		instance = VK_NULL_HANDLE;
		surface = VK_NULL_HANDLE;
		debugMessenger = VK_NULL_HANDLE;
		window = nullptr;
	}

	// clear Engine pointer
	loadedEngine = nullptr;
}

void Engine::run()
{
	LOGFN();

	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit) {
		//begin clock
		const auto start = std::chrono::system_clock::now();

		//everything else

		//get clock again, compare with start clock
		const auto end = std::chrono::system_clock::now();

		//convert to microseconds (integer), and then come back to miliseconds
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		//stats.frametime = elapsed.count() / 1000.f;

		// Handle events on queue
		while (SDL_PollEvent(&e) != 0) {
			// close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_EVENT_QUIT) {
				bQuit = true;
			}

			if (e.type == SDL_EVENT_WINDOW_MINIMIZED) {
				stopRendering = true;
			}
			if (e.type == SDL_EVENT_WINDOW_RESTORED) {
				stopRendering = false;
			}

			ImGui_ImplSDL3_ProcessEvent(&e);
		}

		//do not draw if we are minimized
		if (stopRendering) {
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

		/*ImGui::Begin("Stats");

		ImGui::Text("frametime %f ms", stats.frametime);
		ImGui::Text("draw time %f ms", stats.mesh_draw_time);
		ImGui::Text("update time %f ms", stats.scene_update_time);
		ImGui::Text("triangles %i", stats.triangle_count);
		ImGui::Text("draws %i", stats.drawcall_count);

		ImGui::End();*/

		ImGui::Render();

		//update_scene();

		draw();

		if (resizeRequested) {
			resizeSwapchain();
		}
	}
}

void Engine::draw()
{
	auto &currFrame = getCurrentFrame();

	assert(currFrame.renderFence != VK_NULL_HANDLE);
	// wait until the gpu has finished rendering the last frame. Timeout of 1
	// second
	VK_CHECK(vkWaitForFences(device, 1, &currFrame.renderFence, true, 1000000000));
	currFrame.deletionQueue.flush();
	currFrame.frameDescriptors.clearPools(device);

	assert(device != VK_NULL_HANDLE);
	assert(swapchain != VK_NULL_HANDLE);
	assert(currFrame.swapchainSemaphore != VK_NULL_HANDLE);
	//request image from the swapchain

	uint32_t swapchainImageIndex = -1;
	const VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, 1000000000, currFrame.swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resizeRequested = true;
		return;
	}
	// [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

	VK_CHECK(vkResetFences(device, 1, &currFrame.renderFence));

	//naming it cmd for shorter writing
	const VkCommandBuffer cmd = currFrame.mainCommandBuffer;
	assert(cmd != VK_NULL_HANDLE);

	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	drawExtent.height = std::min(swapchainExtent.height, drawImage.imageExtent.height) * renderScale;
	drawExtent.width = std::min(swapchainExtent.width, drawImage.imageExtent.width) * renderScale;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	drawBackground(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	drawGeometry(cmd); // [RESTORE]

	//transtion the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, drawImage.image, swapchainImages[swapchainImageIndex], drawExtent, swapchainExtent);

	//draw imgui into the swapchain image
	drawImgui(cmd,  swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can show it on the screen
	vkutil::transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue.
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	const VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	const VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, currFrame.swapchainSemaphore);
	const VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currFrame.renderSemaphore);

	const VkSubmitInfo2 submit = vkinit::submit_info(cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, currFrame.renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as its necessary that drawing commands have finished before the image is displayed to the user
	const VkPresentInfoKHR presentInfo {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &currFrame.renderSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &swapchainImageIndex,
	};

	const VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resizeRequested = true;
	}
	// [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

	//increase the number of frames drawn
	frameNumber++;
}

void Engine::drawBackground(VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	//make a clear-color from frame number. This will flash with a 120 frame period.
	const float flash = std::abs(std::sin(frameNumber / 120.f));
	const VkClearColorValue clearValue { { 0.0f, 0.0f, flash, 1.0f } };

	const VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	vkCmdClearColorImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}

void Engine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	assert(cmd != VK_NULL_HANDLE);
	assert(targetImageView != VK_NULL_HANDLE);

	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo renderInfo = vkinit::rendering_info(swapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

// Store the application start time (global or in your initialization code)
const auto startTime = std::chrono::high_resolution_clock::now();

void Engine::drawGeometry(VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	//begin a render pass  connected to our draw image
	const VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	const VkRenderingInfo renderInfo = vkinit::rendering_info(windowExtent, &colorAttachment, &depthAttachment);

	//set dynamic viewport and scissor
	const VkViewport viewport {
		.x = 0,
		.y = 0,
		.width = static_cast<float>(drawExtent.width),
		.height = static_cast<float>(drawExtent.height),
		.minDepth = 0.f,
		.maxDepth = 1.f,
	};

	const VkRect2D scissor {
		.offset = {
			.x = 0,
			.y = 0,
		},
		.extent = {
			.width = drawExtent.width,
			.height = drawExtent.height,
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
		static_cast<float>(drawExtent.width) / static_cast<float>(drawExtent.height),
		0.1f, 10000.0f
		);

	// Invert the Y direction of the projection matrix for OpenGL compatibility
	projection[1][1] *= -1.0f;

	// Push constants with the animated world matrix
	const GPUDrawPushConstants push_constants {
		.worldMatrix = projection * view * worldMatrix,
		.vertexBuffer = meshBuffers.vertexBufferAddress,
	};


	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

	//bind a texture
	VkDescriptorSet imageSet = getCurrentFrame().frameDescriptors.allocate(device, singleImageDescriptorLayout);
	{
		DescriptorWriter writer;

		writer.writeImage(0, errorCheckerboardImage.imageView, defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		writer.updateSet(device, imageSet);
	}
	assert(imageSet != VK_NULL_HANDLE);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdPushConstants(cmd, meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
	vkCmdBindIndexBuffer(cmd, meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

	vkCmdEndRendering(cmd);
}

GPUMeshBuffers Engine::uploadMesh(const std::span<const uint32_t> &indices, const std::span<const Vertex> &vertices)
{
	LOGFN();

	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface = {
		//create index buffer
		.indexBuffer = createBuffer(
			indexBufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY
		),
		//create vertex buffer
		.vertexBuffer = createBuffer(
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
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device, &deviceAdressInfo);

	const AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void *data = get_mapped_data(staging.allocation);
	if (data == nullptr) {
		throw new Failure(FailureType::MappedAccess);
	}

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

	destroyBuffer(staging);

	return newSurface;
}

void Engine::initDefaultData()
{
	LOGFN();

	generateMeshes();

	//3 default textures, white, grey, black. 1 pixel each
	const uint32_t white = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
	const uint32_t black = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
	whiteImage = createImage(reinterpret_cast<const void *>(&white), VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1.f, 0.f, 1.f, 1.f));
	std::array<uint32_t, 16*16> pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	errorCheckerboardImage = createImage(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampl = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
	};

	VK_CHECK(vkCreateSampler(device, &sampl, nullptr, &defaultSamplerNearest));
	if (defaultSamplerNearest == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkSamplerCreation, "Nearest");
	}

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	VK_CHECK(vkCreateSampler(device, &sampl, nullptr, &defaultSamplerLinear));
	if (defaultSamplerLinear == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkSamplerCreation, "Linear");
	}

	mainDeletionQueue.push_function([this]() {
		vkDestroySampler(device, defaultSamplerNearest, nullptr);
		vkDestroySampler(device, defaultSamplerLinear, nullptr);

		destroyImage(whiteImage);
		destroyImage(errorCheckerboardImage);
	});
}

void Engine::createSwapchain(const uint32_t w, const uint32_t h)
{
	LOGFN();

	vkb::SwapchainBuilder swapchainBuilder {
		chosenGPU,
		device,
		surface,
	};

	swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR {
			.format = swapchainImageFormat,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		})
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(w, h)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	//store swapchain and its related images
	swapchain = vkbSwapchain.swapchain;
	swapchainImages = vkbSwapchain.get_images().value();
	swapchainImageViews = vkbSwapchain.get_image_views().value();
	swapchainExtent = vkbSwapchain.extent;

	if (swapchain == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VkSwapchainCreation);
	}
	if (swapchainImages.size() <= 1) {
		throw new Failure(FailureType::VkSwapchainImagesCreation);
	}
}

void Engine::destroySwapchain()
{
	LOGFN();

	vkDestroySwapchainKHR(device, swapchain, nullptr);

	// destroy swapchain resources
	for (auto &iv : swapchainImageViews) {
		vkDestroyImageView(device, iv, nullptr);
	}
}

void Engine::resizeSwapchain()
{
	LOGFN();

	VK_CHECK(vkDeviceWaitIdle(device));

	destroySwapchain();

	int w = -1, h = -1;
	SDL_GetWindowSize(window, &w, &h);
	windowExtent.width = w;
	windowExtent.height = h;

	createSwapchain(windowExtent.width, windowExtent.height);

	resizeRequested = false;
}

AllocatedImage Engine::createImage(const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped)
{
	LOGFN();

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
	VK_CHECK(vmaCreateImage(allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));
	if (newImage.image == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VMAImageCreation);
	}

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	const VkImageAspectFlags aspectFlag = format != VK_FORMAT_D32_SFLOAT
		? VK_IMAGE_ASPECT_COLOR_BIT
		: VK_IMAGE_ASPECT_DEPTH_BIT;

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &newImage.imageView));
	if (newImage.imageView == VK_NULL_HANDLE) {
		throw new Failure(FailureType::VMAImageViewCreation);
	}

	return newImage;
}

AllocatedImage Engine::createImage(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped)
{
	LOGFN();

	assert(data != nullptr);
	assert(format != VK_FORMAT_MAX_ENUM);
	assert(usage != VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM);

	const size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadbuffer = createBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	std::memcpy(uploadbuffer.info.pMappedData, data, data_size);

	AllocatedImage new_image = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

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

		if (mipmapped) {
			vkutil::generate_mipmaps(cmd, new_image.image, VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
		} else {
			vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	});

	destroyBuffer(uploadbuffer);

	return new_image;
}

void Engine::destroyImage(const AllocatedImage &img)
{
	LOGFN();

	vkDestroyImageView(device, img.imageView, nullptr);
	vmaDestroyImage(allocator, img.image, img.allocation);
}

void Engine::generateMeshes()
{
	const std::vector<uint32_t> indices = {0, 1, 3, 0, 3, 2};
	const std::vector<Vertex> vertices {
		{
			.position = {0.f, 0.f, 0.f},
			.uv_x = 0.f,
			.normal = {0.f, 0.f, 1.f},
			.uv_y = 0.f,
			.color = glm::vec4(0.f, 1.f, 0.f, 1.f),
		},
		{
			.position = {0.f, 1.f, 0.f},
			.uv_x = 0.f,
			.normal = {0.f, 0.f, 1.f},
			.uv_y = 1.f,
			.color = glm::vec4(1.f, 0.f, 0.f, 1.f),
		},
		{
			.position = {1.f, 0.f, 0.f},
			.uv_x = 1.f,
			.normal = {0.f, 0.f, 1.f},
			.uv_y = 0.f,
			.color = glm::vec4(0.f, 0.f, 1.f, 1.f),
		},
		{
			.position = {1.f, 1.f, 0.f},
			.uv_x = 1.f,
			.normal = {0.f, 0.f, 1.f},
			.uv_y = 1.f,
			.color = glm::vec4(1.f, 1.f, 0.f, 1.f),
		},
	};

	meshBuffers = uploadMesh(indices, vertices);

	mainDeletionQueue.push_function([&]() {
		destroyBuffer(meshBuffers.indexBuffer);
		destroyBuffer(meshBuffers.vertexBuffer);
	});
}


}
