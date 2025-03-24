#include "engine.h"

#include <iostream>
#include <thread>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include <fmt/printf.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>

#include "../vk/defines.h"
#include "../vk/initializers.h"
#include "../vk/utils.h"


#define LOGFN() {std::cout << __func__ << std::endl;}


constexpr bool bUseValidationLayers = true;


namespace Graphics {

Engine *loadedEngine = nullptr;


Engine &Engine::get()
{
	return *loadedEngine;
}

AllocatedBuffer Engine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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

	AllocatedBuffer newBuffer = {};

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

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

	// only one engine initialization is allowed with the application.
	assert(loadedEngine == nullptr);
	loadedEngine = this;

	initSDL();
	initVulkan();
	initVMA();
	initSwapChain();
	initCommands();
	initSyncStructures();
	initDescriptors();
	initPipelines();
	initImgui();

	//everything went fine apparently
	isInitialized = true;
}

void Engine::initSDL()
{
	LOGFN();

	// We initialize SDL and create a window with it.
	assert(SDL_Init(SDL_INIT_VIDEO));

	const SDL_WindowFlags window_flags = 0
		| SDL_WINDOW_VULKAN
		| SDL_WINDOW_RESIZABLE;

	window = SDL_CreateWindow(
	    "Vulkan Engine",
	    windowExtent.width,
	    windowExtent.height,
	    window_flags
	);

	assert(window != nullptr);
}

void Engine::initVulkan()
{
	LOGFN();

	vkb::InstanceBuilder builder {};

	//make the Vulkan instance, with basic debug features
	const auto inst_ret = builder
		.set_app_name("Example Vulkan Application")
		.request_validation_layers(bUseValidationLayers)
		.require_api_version(1, 3, 0) // We use Vulkan API 1.3, nothing from earlier versions.
		.use_default_debug_messenger()
		.build();

	const vkb::Instance vkb_inst = inst_ret.value();

	//store the instance
	instance = vkb_inst.instance;
	//store the debug messenger
	debugMessenger = vkb_inst.debug_messenger;

	assert(instance != VK_NULL_HANDLE);
	assert(debugMessenger != VK_NULL_HANDLE);

	assert(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));

	constexpr VkPhysicalDeviceVulkan13Features features13 {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
	    .synchronization2 = true,
	    .dynamicRendering = true,
	};

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.3
	vkb::PhysicalDeviceSelector selector {vkb_inst};
	const vkb::PhysicalDevice physicalDevice = selector
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
	vkb::DeviceBuilder deviceBuilder {physicalDevice};

	const vkb::Device vkbDevice = deviceBuilder
		.add_pNext(&bufferDeviceAddressFeatures)
		.build()
		.value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	device = vkbDevice.device;
	chosenGPU = physicalDevice.physical_device;

	graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	assert(surface != VK_NULL_HANDLE);
	assert(device != VK_NULL_HANDLE);
	assert(graphicsQueue != VK_NULL_HANDLE);
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

	mainDeletionQueue.push_function([this]() {
		vmaDestroyAllocator(allocator);
	});
}

void Engine::initSwapChain()
{
	LOGFN();

	createSwapChain(windowExtent.width, windowExtent.height);

	//draw image size will match the window
	// [NOTE] Previously was only windowExtent, but changed to this model in case the size differs.
	const VkExtent3D drawImageExtent {
	    std::min(swapChainExtent.width, windowExtent.width),
	    std::min(swapChainExtent.height, windowExtent.height),
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

	//build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(device, &rview_info, nullptr, &drawImage.imageView));

	//add to deletion queues
	mainDeletionQueue.push_function([this]() {
		vkDestroyImageView(device, drawImage.imageView, nullptr);
		vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
	});

	// Depth IMG
	depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	depthImage.imageExtent = drawImageExtent;
	const VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	const VkImageCreateInfo dimg_info = vkinit::image_create_info(depthImage.imageFormat, depthImageUsages, drawImageExtent);

	//allocate and create the image
	VK_CHECK(vmaCreateImage(allocator, &dimg_info, &rimg_allocinfo, &depthImage.image, &depthImage.allocation, nullptr));

	//build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(device, &dview_info, nullptr, &depthImage.imageView));

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
		VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i]._mainCommandBuffer));

		assert(frames[i]._commandPool != VK_NULL_HANDLE);
		assert(frames[i]._mainCommandBuffer != VK_NULL_HANDLE);
	}

	VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &immCommandPool));

	// allocate the command buffer for immediate submits
	const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &immCommandBuffer));

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
		VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i]._renderFence));

		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i]._renderSemaphore));

		assert(frames[i]._renderFence != VK_NULL_HANDLE);
		assert(frames[i]._swapchainSemaphore != VK_NULL_HANDLE);
		assert(frames[i]._renderSemaphore != VK_NULL_HANDLE);
	}

	VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immFence));
	mainDeletionQueue.push_function([this]() {
		vkDestroyFence(device, immFence, nullptr);
	});
}

void Engine::initDescriptors()
{
	LOGFN();

	//create a descriptor pool that will hold 10 sets with 1 image each
	constexpr DescriptorAllocatorGrowable::PoolSizeRatio sizes[] = {
	                                                                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
	                                                                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
	                                                                };

	globalDescriptorAllocator.init(device, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder{};
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	//allocate a descriptor set for our draw image
	drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

	DescriptorWriter writer{};
	writer.write_image(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(device, drawImageDescriptors);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		constexpr DescriptorAllocatorGrowable::PoolSizeRatio frame_sizes[] = {
		                                                                      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
		                                                                      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
		                                                                      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		                                                                      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		                                                                      };

		frames[i]._frameDescriptors = DescriptorAllocatorGrowable();
		frames[i]._frameDescriptors.init(device, 1000, frame_sizes);

		mainDeletionQueue.push_function([this, i]() {
			frames[i]._frameDescriptors.destroy_pools(device);
		});
	}

	{
		DescriptorLayoutBuilder builder{};
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		gpuSceneDataDescriptorLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	{
		DescriptorLayoutBuilder builder{};
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		singleImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}


	//make sure both the descriptor allocator and the new layout get cleaned up properly
	mainDeletionQueue.push_function([this]() {
		globalDescriptorAllocator.destroy_pools(device);

		vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, gpuSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, singleImageDescriptorLayout, nullptr);
	});

	//metalRoughMaterial.build_pipelines(*this); // [TODO] Look at this
}

void Engine::initPipelines()
{
	initBackgroundPipelines();
}

void Engine::initBackgroundPipelines()
{
	LOGFN();

	const VkPushConstantRange pushConstant {
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

	//layout code
	VkShaderModule computeDrawShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/gradient.comp.spv", device, computeDrawShader)) {
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
	    .layout = gradientPipelineLayout,
	};

	VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientPipeline));

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
	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	assert(ImGui::CreateContext());

	// this initializes imgui for SDL
	assert(ImGui_ImplSDL3_InitForVulkan(window));

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
	        .pColorAttachmentFormats = &swapChainImageFormat,
	    },
	};

	assert(ImGui_ImplVulkan_Init(&init_info));

	assert(ImGui_ImplVulkan_CreateFontsTexture());

	// add the destroy the imgui created structures
	mainDeletionQueue.push_function([this, imguiPool]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(device, imguiPool, nullptr);
	});
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function)
{
	LOGFN();

	VK_CHECK(vkResetFences(device, 1, &immFence));
	VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));

	VkCommandBuffer &cmd = immCommandBuffer;

	const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	const VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, immFence));

	VK_CHECK(vkWaitForFences(device, 1, &immFence, true, 9999999999));
}

void Engine::createSwapChain(const uint32_t w, const uint32_t h)
{
	LOGFN();

	vkb::SwapchainBuilder swapchainBuilder {
	    chosenGPU,
	    device,
	    surface,
	};

	swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
	                                  .use_default_format_selection()
	                                  .set_desired_format(VkSurfaceFormatKHR {
	                                      .format = swapChainImageFormat,
	                                      .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	                                  })
	                                  //use vsync present mode
	                                  .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
	                                  .set_desired_extent(w, h)
	                                  .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	                                  .build()
	                                  .value();

	//store swapchain and its related images
	swapChain = vkbSwapchain.swapchain;
	swapChainImages = vkbSwapchain.get_images().value();
	swapChainImageViews = vkbSwapchain.get_image_views().value();
	swapChainExtent = vkbSwapchain.extent;

	assert(swapChain != VK_NULL_HANDLE);
	assert(swapChainImages.size() > 1);
	assert(swapChainImages.size() == swapChainImageViews.size());
}

void Engine::destroySwapChain()
{
	LOGFN();

	vkDestroySwapchainKHR(device, swapChain, nullptr);

	// destroy swapchain resources
	for (auto &iv : swapChainImageViews) {
		vkDestroyImageView(device, iv, nullptr);
	}
}

void Engine::resizeSwapChain()
{
	LOGFN();

	VK_CHECK(vkDeviceWaitIdle(device));

	destroySwapChain();

	int w = -1, h = -1;
	SDL_GetWindowSize(window, &w, &h);
	windowExtent.width = w;
	windowExtent.height = h;

	createSwapChain(windowExtent.width, windowExtent.height);

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

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	const VkImageAspectFlags aspectFlag = format != VK_FORMAT_D32_SFLOAT
	                                          ? VK_IMAGE_ASPECT_COLOR_BIT
	                                          : VK_IMAGE_ASPECT_DEPTH_BIT;

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &newImage.imageView));

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

	immediateSubmit([&](VkCommandBuffer cmd) {
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
		stats.frametime = elapsed.count() / 1000.f;

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
			ImGui::SliderFloat("Render Scale", &renderingScale, 0.3f, 1.f);
			ImGui::End();
		}

		ImGui::Begin("Stats");

		ImGui::Text("frametime %f ms", stats.frametime);
		ImGui::Text("draw time %f ms", stats.mesh_draw_time);
		ImGui::Text("update time %f ms", stats.scene_update_time);
		ImGui::Text("triangles %i", stats.triangle_count);
		ImGui::Text("draws %i", stats.drawcall_count);

		ImGui::End();

		ImGui::Render();

		draw();

		if (resizeRequested) {
			resizeSwapChain();
		}
	}
}

void Engine::cleanup()
{
	LOGFN();

	if (isInitialized) {
		// We need to wait fort he GPU to finish until...
		VK_CHECK(vkDeviceWaitIdle(device));

		//loadedScenes.clear();

		// we can destroy the command pools.
		// It may crash the app otherwise.
		/*for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
		}*/
		for (int i = 0; i < FRAME_OVERLAP; i++) {
			//already written from before
			vkDestroyCommandPool(device, frames[i]._commandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(device, frames[i]._renderFence, nullptr);
			vkDestroySemaphore(device, frames[i]._renderSemaphore, nullptr);
			vkDestroySemaphore(device, frames[i]._swapchainSemaphore, nullptr);

			frames[i]._deletionQueue.flush();

			frames[i]._commandPool = VK_NULL_HANDLE;
			frames[i]._renderFence = VK_NULL_HANDLE;
			frames[i]._renderSemaphore = VK_NULL_HANDLE;
			frames[i]._swapchainSemaphore = VK_NULL_HANDLE;
		}

		/*for (auto &mesh : testMeshes) {
			destroy_buffer(mesh->meshBuffers.indexBuffer);
			destroy_buffer(mesh->meshBuffers.vertexBuffer);
		}*/

		//flush the global deletion queue
		mainDeletionQueue.flush();

#ifdef VMA_USE_DEBUG_LOG
		// Right here because all VMA (de)allocs must all have been performed
		// just when we finish flushing the main queue.
		assert(allocationCounter == 0 && "Memory leak detected!");
#endif

		//destroy swapchain-associated resources
		destroySwapChain();

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

	// clear engine pointer
	loadedEngine = nullptr;
}

void Engine::draw()
{
	LOGFN();

	auto &_currFrame = getCurrentFrame();

	assert(_currFrame._renderFence != VK_NULL_HANDLE);
	// wait until the gpu has finished rendering the last frame. Timeout of 1
	// second
	VK_CHECK(vkWaitForFences(device, 1, &_currFrame._renderFence, true, 1000000000));
	_currFrame._deletionQueue.flush();
	_currFrame._frameDescriptors.clear_pools(device);

	assert(device != VK_NULL_HANDLE);
	assert(swapChain != VK_NULL_HANDLE);
	assert(_currFrame._swapchainSemaphore != VK_NULL_HANDLE);
	//request image from the swapchain

	uint32_t swapchainImageIndex = -1;
	const VkResult acquireResult = vkAcquireNextImageKHR(device, swapChain, 1000000000, _currFrame._swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resizeRequested = true;
		return;
	}
	// [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

	VK_CHECK(vkResetFences(device, 1, &_currFrame._renderFence));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = _currFrame._mainCommandBuffer;
	assert(cmd != VK_NULL_HANDLE);

	assert(cmd != VK_NULL_HANDLE);
	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	drawExtent.height = std::min(swapChainExtent.height, drawImage.imageExtent.height) * renderingScale;
	drawExtent.width= std::min(swapChainExtent.width, drawImage.imageExtent.width) * renderingScale;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	//draw_pc(cmd);
	drawBackground(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	//draw_geometry(cmd); [TODO] See how to handle this.

	//transtion the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, swapChainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, drawImage.image, swapChainImages[swapchainImageIndex], drawExtent, swapChainExtent);

	//draw imgui into the swapchain image
	drawImgui(cmd,  swapChainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can show it on the screen
	vkutil::transition_image(cmd, swapChainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
	VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, _currFrame._renderFence));

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
	    .pSwapchains = &swapChain,
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
	LOGFN();

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
	LOGFN();

	assert(cmd != VK_NULL_HANDLE);
	assert(targetImageView != VK_NULL_HANDLE);

	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo renderInfo = vkinit::rendering_info(swapChainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

}
