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

#include "../states.h"

#include "../world/scene.h"

#include "defines.h"
#include "failure.h"
#include "initializers.h"
#include "pipelinebuilder.h"
#include "utils.h"
#include "vma.h"

#define LOGFN() \
    { \
        std::cout << __func__ << '\n'; \
    }

namespace {

constexpr bool bUseValidationLayers = true;

} // namespace

namespace Graphics {

Engine *Engine::loadedEngine = nullptr;
decltype(std::chrono::system_clock::now()) Engine::prevChrono = std::chrono::system_clock::now();

constexpr glm::mat4 createOrthographicProjection(const float left, const float right, const float bottom, const float top)
{
	const float h = (top - bottom);
	const float w = (right - left);

	glm::mat4 projection = glm::mat4(1.f);
	projection[0][0] = 2.f / w;
	projection[1][1] = 2.f / h;
	projection[2][2] = 1.f;  // Z for layer ordering

	projection[3][0] = -(right + left) / (right - left);
	projection[3][1] = -(top + bottom) / (top - bottom);

	return projection;
}

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
	VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));
	if (newBuffer.buffer == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkBufferAllocation);
	}

	return newBuffer;
}

void Engine::destroyBuffer(const AllocatedBuffer &buffer)
{
	LOGFN();

	vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
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
	m_isInitialized = true;
}

void Engine::initSDL()
{
	LOGFN();

	// We initialize SDL and create a window with it.
	const bool sdlInitd = SDL_Init(SDL_INIT_VIDEO);
	if (!sdlInitd) {
		throw Failure(FailureType::SDLInitialisation);
	}

	constexpr SDL_WindowFlags window_flags = 0
		| SDL_WINDOW_VULKAN
		| SDL_WINDOW_RESIZABLE;

	m_window = SDL_CreateWindow(
		"Vulkan Engine",
		static_cast<int>(m_windowExtent.width),
		static_cast<int>(m_windowExtent.height),
		window_flags
	);

	if (m_window == nullptr) {
		throw Failure(FailureType::SDLWindowCreation);
	}
}

void enumerateDevices(VkInstance inst)
{
    uint32_t gpuCount;
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &gpuCount, nullptr));

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &gpuCount, gpus.data()));

    VkPhysicalDeviceProperties props{};
    for (const auto &gpu : gpus) {
        vkGetPhysicalDeviceProperties(gpu, &props);

        std::cout << "Available GPU: " << props.deviceName << '(' << props.deviceID << ':'
                  << props.deviceType << ':' << props.apiVersion << ':' << props.driverVersion
                  << ")\n";
    }
}

void Engine::initVulkan()
{
    LOGFN();

    //make the Vulkan instance, with basic debug features
    const auto instRet
        = vkb::InstanceBuilder()
              .set_app_name("Example Vulkan Application")
              .request_validation_layers(bUseValidationLayers)
              .require_api_version(1,
                                   3,
                                   0) // We use Vulkan API 1.3, nothing from earlier versions.
              .enable_layer("VK_LAYER_KHRONOS_validation")
              .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
              .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
              .use_default_debug_messenger()
              .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
              .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
              .build();

    const vkb::Instance vkb_inst = instRet.value();

    //store the instance
    m_instance = vkb_inst.instance;
    if (m_instance == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkInstanceCreation);
    }
    //store the debug messenger
    m_debugMessenger = vkb_inst.debug_messenger;
    if (m_debugMessenger == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkDebugMessengerCreation);
    }

    const bool surfaceCreated = SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface);
    if (!surfaceCreated) {
        throw Failure(FailureType::VkSurfaceCreation1);
    }

    constexpr VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    //use vkbootstrap to select a GPU.
    //We want a GPU that can write to the SDL m_surface and supports Vulkan 1.3
    const vkb::PhysicalDevice physicalDevice = vkb::PhysicalDeviceSelector(vkb_inst)
                                                   .set_minimum_version(1,
                                                                        3) // We run on Vulkan 1.3+
                                                   .set_required_features_13(features13)
                                                   .set_surface(m_surface)
                                                   .select()
                                                   .value();

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
    };

    enumerateDevices(m_instance);

    //create the final Vulkan device
    const vkb::Device vkbDevice
        = vkb::DeviceBuilder(physicalDevice).add_pNext(&bufferDeviceAddressFeatures).build().value();

    // Get the VkDevice handle used in the rest of a Vulkan application
    m_device = vkbDevice.device;
    m_chosenGPU = physicalDevice.physical_device;

    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(m_chosenGPU, &props);

        std::cout << "Chosen GPU: " << props.deviceName << '(' << props.deviceID << ':'
                  << props.deviceType << ':' << props.apiVersion << ':' << props.driverVersion
                  << ")\n";
    }

    m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    if (m_surface == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkSurfaceCreation2);
    }
    if (m_device == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkDeviceCreation);
    }
    if (m_graphicsQueue == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkQueueCreation);
    }
}

void Engine::initVMA()
{
	LOGFN();

	// initialize the memory allocator
    const VmaAllocatorCreateInfo allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m_chosenGPU,
        .device = m_device,
        .instance = m_instance,
        .vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0),
    };

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocator));
    if (m_allocator == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAInitialisation);
    }

    m_mainDeletionQueue.push_function([this]() { vmaDestroyAllocator(m_allocator); });
}

void Engine::initSwapchain()
{
	LOGFN();

	createSwapchain(m_windowExtent.width, m_windowExtent.height);

	//draw image size will match the window
	// [NOTE] Previously was only m_windowExtent, but changed to this model in case the size differs.
	const VkExtent3D drawImageExtent {
		std::min(m_swapchainExtent.width, m_windowExtent.width),
		std::min(m_swapchainExtent.height, m_windowExtent.height),
		1,
	};

	// Color IMG
	//hardcoding the draw format to 32 bit float
	m_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_drawImage.imageExtent = drawImageExtent;

	constexpr VkImageUsageFlags drawImageUsages = 0
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const VkImageCreateInfo rimg_info = vkinit::image_create_info(m_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	const VmaAllocationCreateInfo rimg_allocinfo {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};

	//allocate and create the image
	VK_CHECK(vmaCreateImage(m_allocator, &rimg_info, &rimg_allocinfo, &m_drawImage.image, &m_drawImage.allocation, nullptr));
	if (m_drawImage.image == VK_NULL_HANDLE) {
		throw Failure(FailureType::VMAImageCreation, "Drawing");
	}

	//build a image-view for the draw image to use for rendering
	const VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(m_drawImage.imageFormat, m_drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(m_device, &rview_info, nullptr, &m_drawImage.imageView));
	if (m_drawImage.imageView == VK_NULL_HANDLE) {
		throw Failure(FailureType::VMAImageViewCreation, "Drawing");
	}

	//add to deletion queues
    m_mainDeletionQueue.push_function([this]() {
        vkDestroyImageView(m_device, m_drawImage.imageView, nullptr);
        vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation);
    });

    // Depth IMG
    m_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_depthImage.imageExtent = drawImageExtent;
    constexpr VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkImageCreateInfo dimg_info = vkinit::image_create_info(m_depthImage.imageFormat,
                                                                  depthImageUsages,
                                                                  drawImageExtent);

    //allocate and create the image
    VK_CHECK(vmaCreateImage(m_allocator,
                            &dimg_info,
                            &rimg_allocinfo,
                            &m_depthImage.image,
                            &m_depthImage.allocation,
                            nullptr));
    if (m_depthImage.image == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAImageCreation, "Depth");
    }

    //build a image-view for the draw image to use for rendering
    const VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(m_depthImage.imageFormat,
                                                                           m_depthImage.image,
                                                                           VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(m_device, &dview_info, nullptr, &m_depthImage.imageView));
    if (m_depthImage.imageView == VK_NULL_HANDLE) {
		throw Failure(FailureType::VMAImageViewCreation, "Depth");
	}

    m_mainDeletionQueue.push_function([this]() {
        vkDestroyImageView(m_device, m_depthImage.imageView, nullptr);
        vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation);
    });
}

void Engine::initCommands()
{
	LOGFN();

	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	const VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (uint i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_frames[i].commandPool));
		if (m_frames[i].commandPool == VK_NULL_HANDLE) {
			throw Failure(FailureType::VkCommandPoolCreation, "Frame");
		}

		// allocate the default command buffer that we will use for rendering
		const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_frames[i].commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_frames[i].mainCommandBuffer));
		if (m_frames[i].mainCommandBuffer == VK_NULL_HANDLE) {
			throw Failure(FailureType::VkCommandBufferCreation, "Frame");
		}
	}

	VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_immCommandPool));
	if (m_immCommandPool == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkCommandPoolCreation, "Immediate");
	}

	// allocate the command buffer for immediate submits
	const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_immCommandBuffer));
	if (m_immCommandBuffer == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkCommandBufferCreation, "Immediate");
	}

    m_mainDeletionQueue.push_function(
        [this]() { vkDestroyCommandPool(m_device, m_immCommandPool, nullptr); });
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

	for (uint i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_frames[i].renderFence));
		if (m_frames[i].renderFence == VK_NULL_HANDLE) {
			throw Failure(FailureType::VkFenceCreation, "Frame");
		}

		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].swapchainSemaphore));
		if (m_frames[i].swapchainSemaphore == VK_NULL_HANDLE) {
			throw Failure(FailureType::VkSwapchainCreation, "Frame");
		}
		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frames[i].renderSemaphore));
		if (m_frames[i].renderSemaphore == VK_NULL_HANDLE) {
			throw Failure(FailureType::VkSemaphoreCreation, "Frame");
		}
	}

	VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_immFence));
	if (m_immFence == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkFenceCreation, "Immediate");
	}

    m_mainDeletionQueue.push_function([this]() { vkDestroyFence(m_device, m_immFence, nullptr); });
}

void Engine::initDescriptors()
{
	LOGFN();

	//create a descriptor pool that will hold 10 sets with 1 image each
	constexpr DescriptorAllocatorGrowable::PoolSizeRatio sizes[] {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
	};

	m_globalDescriptorAllocator.init(m_device, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder {};
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		m_drawImageDescriptorLayout = builder.build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
		if (m_drawImageDescriptorLayout == VK_NULL_HANDLE) {
			throw Failure(FailureType::VkDescriptorCreation, "Draw");
		}
	}

	//allocate a descriptor set for our draw image
	m_drawImageDescriptors = m_globalDescriptorAllocator.allocate(m_device, m_drawImageDescriptorLayout);

	DescriptorWriter writer {};
	writer.writeImage(0, m_drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.updateSet(m_device, m_drawImageDescriptors);
	if (m_drawImageDescriptors == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkDescriptorUpdate, "Draw");
	}

	for (uint i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		constexpr DescriptorAllocatorGrowable::PoolSizeRatio frame_sizes[] = {
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		};

		m_frames[i].frameDescriptors = DescriptorAllocatorGrowable();
		m_frames[i].frameDescriptors.init(m_device, 1000, frame_sizes);

        m_mainDeletionQueue.push_function(
            [this, i]() { m_frames[i].frameDescriptors.destroyPools(m_device); });
    }

    {
        DescriptorLayoutBuilder builder{};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        m_gpuSceneDataDescriptorLayout = builder.build(m_device,
                                                     VK_SHADER_STAGE_VERTEX_BIT
                                                         | VK_SHADER_STAGE_FRAGMENT_BIT);
        if (m_gpuSceneDataDescriptorLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkDescriptorLayoutCreation, "GPU");
        }
    }

    {
        DescriptorLayoutBuilder builder{};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        m_singleImageDescriptorLayout = builder.build(m_device, VK_SHADER_STAGE_FRAGMENT_BIT);
        if (m_singleImageDescriptorLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkDescriptorLayoutCreation, "Single");
        }
    }

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    m_mainDeletionQueue.push_function([this]() {
        m_globalDescriptorAllocator.destroyPools(m_device);

        vkDestroyDescriptorSetLayout(m_device, m_drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_gpuSceneDataDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_singleImageDescriptorLayout, nullptr);
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
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/tex_image.frag.spv", m_device, triangleFragShader)) {
		throw Failure(FailureType::FragmentShader, "Triangle");
	}

	VkShaderModule triangleVertexShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/colored_triangle_mesh.vert.spv", m_device, triangleVertexShader)) {
		throw Failure(FailureType::VertexShader, "Triangle");
	}

	const VkPushConstantRange bufferRange {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUDrawPushConstants),
	};

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = &m_singleImageDescriptorLayout;
	pipeline_layout_info.setLayoutCount = 1;

	VK_CHECK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_meshPipelineLayout));
	if (m_meshPipelineLayout == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkPipelineLayoutCreation);
	}

	PipelineBuilder pipelineBuilder {};
	//use the triangle layout we created
	pipelineBuilder.pipelineLayout = m_meshPipelineLayout;
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
	pipelineBuilder.setColorAttachmentFormat(m_drawImage.imageFormat);
	pipelineBuilder.setDepthFormat(m_depthImage.imageFormat);

	//finally build the pipeline
	m_meshPipeline = pipelineBuilder.buildPipeline(m_device);
	if (m_meshPipeline == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkPipelineCreation);
	}

	//clean structures
	vkDestroyShaderModule(m_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(m_device, triangleVertexShader, nullptr);

    m_mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(m_device, m_meshPipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_meshPipeline, nullptr);
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
		.pSetLayouts = &m_drawImageDescriptorLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstant,
	};

	VK_CHECK(vkCreatePipelineLayout(m_device, &computeLayout, nullptr, &m_gradientPipelineLayout));
	if (m_gradientPipelineLayout == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkPipelineLayoutCreation, "Gradient");
	}

	//layout code
	VkShaderModule computeDrawShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/gradient.comp.spv", m_device, computeDrawShader)) {
		throw Failure(FailureType::ComputeShader);
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
		.layout = m_gradientPipelineLayout,
	};

	VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_gradientPipeline));
	if (m_gradientPipeline == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkPipelineCreation, "Gradient");
	}

	vkDestroyShaderModule(m_device, computeDrawShader, nullptr);

    m_mainDeletionQueue.push_function([this]() {
        vkDestroyPipelineLayout(m_device, m_gradientPipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_gradientPipeline, nullptr);
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
	VK_CHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &imguiPool));
	if (imguiPool == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkDescriptorPoolCreation);
	}

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	const auto igCtx = ImGui::CreateContext();
	if (igCtx == nullptr) {
		throw Failure(FailureType::ImguiContext);
	}

	// this initializes imgui for SDL
	const bool igS3VkInited = ImGui_ImplSDL3_InitForVulkan(m_window);
	if (!igS3VkInited) {
		throw Failure(FailureType::ImguiInitialisation);
	}

	// this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info {
		.Instance = m_instance,
		.PhysicalDevice = m_chosenGPU,
		.Device = m_device,
		.Queue = m_graphicsQueue,
		.DescriptorPool = imguiPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.UseDynamicRendering = true,

		//dynamic rendering parameters for imgui to use
		.PipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &m_swapchainImageFormat,
		},
	};

    const bool igVkInited = ImGui_ImplVulkan_Init(&init_info);
	if (!igVkInited) {
		throw Failure(FailureType::ImguiVkInitialisation);
	}

	const bool igFtInited = ImGui_ImplVulkan_CreateFontsTexture();
	if (!igFtInited) {
		throw Failure(FailureType::ImguiFontsInitialisation);
	}

	// add the destroy the imgui created structures
    m_mainDeletionQueue.push_function([this, imguiPool]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(m_device, imguiPool, nullptr);
    });
}

void Engine::immediate_submit(const std::function<void(VkCommandBuffer cmd)> &function)
{
	LOGFN();

	assert(m_immFence);
	assert(m_immCommandBuffer);

	VK_CHECK(vkResetFences(m_device, 1, &m_immFence));
	VK_CHECK(vkResetCommandBuffer(m_immCommandBuffer, 0));

	VkCommandBuffer cmd = m_immCommandBuffer;

	const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	const VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	const VkSubmitInfo2 submit = vkinit::submit_info(cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submit, m_immFence));

	VK_CHECK(vkWaitForFences(m_device, 1, &m_immFence, true, 9999999999));
}

void Engine::cleanup()
{
	LOGFN();

	if (m_isInitialized) {
		// We need to wait fort he GPU to finish until...
		VK_CHECK(vkDeviceWaitIdle(m_device));

		// we can destroy the command pools.
		// It may crash the app otherwise.
		for (uint i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(m_device, m_frames[i].commandPool, nullptr);
		}
		for (uint i = 0; i < FRAME_OVERLAP; i++) {
			//destroy sync objects
			vkDestroyFence(m_device, m_frames[i].renderFence, nullptr);
			vkDestroySemaphore(m_device, m_frames[i].renderSemaphore, nullptr);
			vkDestroySemaphore(m_device, m_frames[i].swapchainSemaphore, nullptr);

			m_frames[i].deletionQueue.flush();
		}

        //flush the global deletion queue
        m_mainDeletionQueue.flush();

#ifdef VMA_USE_DEBUG_LOG
		// Right here because all VMA (de)allocs must all have been performed
		// just when we finish flushing the main queue.
		assert(allocationCounter == 0 && "Memory leak detected!");
#endif

        //destroy swapchain-associated resources
        destroySwapchain();

        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
        vkDestroyInstance(m_instance, nullptr);
        SDL_DestroyWindow(m_window);

        m_device = VK_NULL_HANDLE;
        m_instance = VK_NULL_HANDLE;
        m_surface = VK_NULL_HANDLE;
        m_debugMessenger = VK_NULL_HANDLE;
        m_window = nullptr;
    }

    // clear Engine pointer
    loadedEngine = nullptr;
}

void Engine::run(const std::function<void()> &prepare, std::atomic<uint64_t> *commands)
{
	LOGFN();

    prevChrono = std::chrono::system_clock::now();
    SDL_Event e;

    prepare();

    // main loop
    while (!(*commands & CommandStates::Stop)) {
        const auto currentTime = std::chrono::system_clock::now();
        const auto delta = currentTime - prevChrono;
        m_deltaMS = static_cast<double>(
                      std::chrono::duration_cast<std::chrono::milliseconds>(delta).count())
                  / 1000.0;

        //convert to microseconds (integer), and then come back to miliseconds
        const auto frametime = std::chrono::duration_cast<std::chrono::microseconds>(delta).count()
                               / 1000.f;

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_EVENT_QUIT) {
                *commands |= CommandStates::Stop;
            }

            if (e.type == SDL_EVENT_WINDOW_MINIMIZED) {
                m_stopRendering = true;
            }
            if (e.type == SDL_EVENT_WINDOW_RESTORED) {
                m_stopRendering = false;
            }

            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        //do not draw if we are minimized
        if (m_stopRendering) {
            //throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &m_renderScale, 0.3f, 1.f);

            /*ComputeEffect &selected = backgroundEffects[currentBackgroundEffect];

			ImGui::Text("Selected effect: %s", selected.name);

			ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

			ImGui::InputFloat4("data1", reinterpret_cast<float *>(&selected.data.data1));
			ImGui::InputFloat4("data2", reinterpret_cast<float *>(&selected.data.data2));
			ImGui::InputFloat4("data3", reinterpret_cast<float *>(&selected.data.data3));
			ImGui::InputFloat4("data4", reinterpret_cast<float *>(&selected.data.data4));*/

			ImGui::End();
        }

        ImGui::Begin("Stats");
        ImGui::Text("frametime %f ms", frametime);
        ImGui::End();

        ImGui::Render();

        // Request the data to be updated before drawing.
        *commands |= CommandStates::PrepareDrawing;
        while (!(*commands & CommandStates::DrawingPrepared)) {
            // Wait for the submitted work request to be performed & finished.
            // As this loop runs for the rendering, there are no reasons to
            // redraw what's already on the screen. The only reason would be for
            // the animations. So far, this runs smoothly enough.
        }
        // Reset state.
        *commands &= ~CommandStates::DrawingPrepared;

        updateAnimations(*m_scene);
        draw();

        if (m_resizeRequested) {
            resizeSwapchain();
        }

        prevChrono = currentTime;
    }
}

void Engine::updateAnimations(World::Scene &scene)
{
    for (auto &chunk : scene.view) {
        for (size_t i = 0; i < chunk.animFrames.size(); i++) {
            chunk.animFrames[i] += m_deltaMS;
        }
    }
}

void Engine::draw()
{
    auto &currFrame = getCurrentFrame();

    assert(currFrame.renderFence != VK_NULL_HANDLE);
    // wait until the gpu has finished rendering the last frame. Timeout of 1
    // second
    VK_CHECK(vkWaitForFences(m_device, 1, &currFrame.renderFence, true, 1000000000));
    currFrame.deletionQueue.flush();
    currFrame.frameDescriptors.clearPools(m_device);

    assert(m_device != VK_NULL_HANDLE);
    assert(m_swapchain != VK_NULL_HANDLE);
    assert(currFrame.swapchainSemaphore != VK_NULL_HANDLE);
    //request image from the swapchain

    uint32_t swapchainImageIndex = -1;
    const VkResult acquireResult = vkAcquireNextImageKHR(m_device,
                                                         m_swapchain,
                                                         1000000000,
                                                         currFrame.swapchainSemaphore,
                                                         nullptr,
                                                         &swapchainImageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_resizeRequested = true;
        return;
    }
    // [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

    VK_CHECK(vkResetFences(m_device, 1, &currFrame.renderFence));

    //naming it cmd for shorter writing
    VkCommandBuffer cmd = currFrame.mainCommandBuffer;
    assert(cmd != VK_NULL_HANDLE);

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    m_drawExtent.height = static_cast<uint32_t>(
        static_cast<float>(std::min(m_swapchainExtent.height, m_drawImage.imageExtent.height))
        * m_renderScale);
    m_drawExtent.width = static_cast<uint32_t>(static_cast<float>(std::min(m_swapchainExtent.width, m_drawImage.imageExtent.width)) * m_renderScale);

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	drawBackground(cmd);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	drawGeometry(cmd); // [RESTORE]

	//transtion the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, m_drawImage.image, m_swapchainImages[swapchainImageIndex], m_drawExtent, m_swapchainExtent);

	//draw imgui into the swapchain image
	drawImgui(cmd,  m_swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can show it on the screen
	vkutil::transition_image(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
	VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submit, currFrame.renderFence));

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
		.pSwapchains = &m_swapchain,
		.pImageIndices = &swapchainImageIndex,
	};

	const VkResult presentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		m_resizeRequested = true;
	}
	// [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

	//increase the number of frames drawn
    m_frameNumber++;
}

void Engine::drawBackground(VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	//make a clear-color from frame number. This will flash with a 120 frame period.
	const float flash = std::abs(std::sin(static_cast<float>(m_frameNumber) / 120.f));
	const VkClearColorValue clearValue { { 0.0f, 0.0f, flash, 1.0f } };

	const VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	//clear image
	vkCmdClearColorImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}

void Engine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	assert(cmd != VK_NULL_HANDLE);
	assert(targetImageView != VK_NULL_HANDLE);

	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo renderInfo = vkinit::rendering_info(m_swapchainExtent, &colorAttachment, nullptr);

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
	const VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	const VkRenderingInfo renderInfo = vkinit::rendering_info(m_windowExtent, &colorAttachment, &depthAttachment);

	//set dynamic viewport and scissor
	const VkViewport viewport {
		.x = 0,
		.y = 0,
		.width = static_cast<float>(m_drawExtent.width),
		.height = static_cast<float>(m_drawExtent.height),
		.minDepth = 0.f,
		.maxDepth = 1.f,
	};

	const VkRect2D scissor {
		.offset = {
			.x = 0,
			.y = 0,
		},
		.extent = {
			.width = m_drawExtent.width,
			.height = m_drawExtent.height,
		},
	};

	vkCmdBeginRendering(cmd, &renderInfo);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshPipeline);

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// Naive impl for now.
	if (m_scene) {
		const auto worldMatrix = createOrthographicProjection(-80.f, 80.f, 50.f, -50.f);

		GPUDrawPushConstants push_constants {
			.vertexBuffer = m_scene->res->meshBuffers.vertexBufferAddress,
		};

		for (const auto &chunk : m_scene->view) {
			const size_t size = chunk.descriptions.size();
            for (size_t i = 0; i < size; i++) {
                const auto descId = chunk.descriptions[i];
                push_constants.frameInterval = m_scene->res->animInterval[descId];
                push_constants.framesCount = m_scene->res->animFrames[descId];
                push_constants.gridColumns = m_scene->res->animColumns[descId];
                push_constants.gridRows = m_scene->res->animRows[descId];

                push_constants.animationTime = chunk.animFrames[i]; // current frame
                push_constants.worldMatrix = glm::translate(worldMatrix, chunk.positions[i])
                                             * chunk.transforms[i];

                //bind the texture
                VkDescriptorSet imageSet
                    = getCurrentFrame().frameDescriptors.allocate(m_device,
                                                                  m_singleImageDescriptorLayout);
                {
                    DescriptorWriter writer;

                    writer.writeImage(0,
                                      m_scene->res->images[chunk.descriptions[i]].imageView,
                                      m_defaultSamplerNearest,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                    writer.updateSet(m_device, imageSet);
                }
                assert(imageSet != VK_NULL_HANDLE);

                vkCmdBindDescriptorSets(cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        m_meshPipelineLayout,
                                        0,
                                        1,
                                        &imageSet,
                                        0,
                                        nullptr);

                vkCmdPushConstants(cmd,
                                   m_meshPipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   0,
                                   sizeof(GPUDrawPushConstants),
                                   &push_constants);
                vkCmdBindIndexBuffer(cmd,
                                     m_scene->res->meshBuffers.indexBuffer.buffer,
                                     0,
                                     VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(cmd, 6, 1, chunk.descriptions[i] * 6, 0, 0);
            }
        }
    } else {
        const GPUDrawPushConstants push_constants{
            .worldMatrix = createOrthographicProjection(-80.f, 80.f, -50.f, 50.f),
            .vertexBuffer = m_meshBuffers.vertexBufferAddress,
        };

        //bind a texture
        VkDescriptorSet imageSet
            = getCurrentFrame().frameDescriptors.allocate(m_device, m_singleImageDescriptorLayout);
        {
            DescriptorWriter writer;

            writer.writeImage(0,
                              m_errorCheckerboardImage.imageView,
                              m_defaultSamplerNearest,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            writer.updateSet(m_device, imageSet);
        }
        assert(imageSet != VK_NULL_HANDLE);

        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_meshPipelineLayout,
                                0,
                                1,
                                &imageSet,
                                0,
                                nullptr);

        vkCmdPushConstants(cmd,
                           m_meshPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(GPUDrawPushConstants),
                           &push_constants);
        vkCmdBindIndexBuffer(cmd, m_meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
    }

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
	newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_device, &deviceAdressInfo);

	const AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void *data = get_mapped_data(staging.allocation);
	if (data == nullptr) {
		throw Failure(FailureType::MappedAccess);
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
	m_whiteImage = createImage(reinterpret_cast<const void *>(&white), VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1.f, 0.f, 1.f, 1.f));
	std::array<uint32_t, static_cast<size_t>(16*16)> pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	m_errorCheckerboardImage = createImage(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampl = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
	};

	VK_CHECK(vkCreateSampler(m_device, &sampl, nullptr, &m_defaultSamplerNearest));
	if (m_defaultSamplerNearest == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkSamplerCreation, "Nearest");
	}

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	VK_CHECK(vkCreateSampler(m_device, &sampl, nullptr, &m_defaultSamplerLinear));
	if (m_defaultSamplerLinear == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkSamplerCreation, "Linear");
	}

    m_mainDeletionQueue.push_function([this]() {
        vkDestroySampler(m_device, m_defaultSamplerNearest, nullptr);
        vkDestroySampler(m_device, m_defaultSamplerLinear, nullptr);

        destroyImage(m_whiteImage);
        destroyImage(m_errorCheckerboardImage);
    });
}

void Engine::createSwapchain(const uint32_t w, const uint32_t h)
{
	LOGFN();

	vkb::SwapchainBuilder swapchainBuilder {
		m_chosenGPU,
		m_device,
		m_surface,
	};

	m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR {
			.format = m_swapchainImageFormat,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		})
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(w, h)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	//store swapchain and its related images
	m_swapchain = vkbSwapchain.swapchain;
	m_swapchainImages = vkbSwapchain.get_images().value();
	m_swapchainImageViews = vkbSwapchain.get_image_views().value();
	m_swapchainExtent = vkbSwapchain.extent;

	if (m_swapchain == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkSwapchainCreation);
	}
	if (m_swapchainImages.size() <= 1) {
		throw Failure(FailureType::VkSwapchainImagesCreation);
	}
}

void Engine::destroySwapchain()
{
	LOGFN();

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

	// destroy swapchain resources
	for (auto &iv : m_swapchainImageViews) {
		vkDestroyImageView(m_device, iv, nullptr);
	}
}

void Engine::resizeSwapchain()
{
	LOGFN();

	VK_CHECK(vkDeviceWaitIdle(m_device));

	destroySwapchain();

	int w = -1, h = -1;
	SDL_GetWindowSize(m_window, &w, &h);
	m_windowExtent.width = w;
	m_windowExtent.height = h;

	createSwapchain(m_windowExtent.width, m_windowExtent.height);

	m_resizeRequested = false;
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
	VK_CHECK(vmaCreateImage(m_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));
	if (newImage.image == VK_NULL_HANDLE) {
		throw Failure(FailureType::VMAImageCreation);
	}

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	const VkImageAspectFlags aspectFlag = format != VK_FORMAT_D32_SFLOAT
		? VK_IMAGE_ASPECT_COLOR_BIT
		: VK_IMAGE_ASPECT_DEPTH_BIT;

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(m_device, &view_info, nullptr, &newImage.imageView));
	if (newImage.imageView == VK_NULL_HANDLE) {
		throw Failure(FailureType::VMAImageViewCreation);
	}

	return newImage;
}

AllocatedImage Engine::createImage(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped)
{
	LOGFN();

	assert(data != nullptr);
	assert(format != VK_FORMAT_MAX_ENUM);
	assert(usage != VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM);

	const size_t data_size = static_cast<size_t>(size.depth) * static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * 4;
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

	vkDestroyImageView(m_device, img.imageView, nullptr);
	vmaDestroyImage(m_allocator, img.image, img.allocation);
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

	m_meshBuffers = uploadMesh(indices, vertices);

    m_mainDeletionQueue.push_function([&]() {
        destroyBuffer(m_meshBuffers.indexBuffer);
        destroyBuffer(m_meshBuffers.vertexBuffer);
    });
}


void Engine::setScene(World::Scene &scene)
{
	m_scene = &scene;
}

} // namespace Graphics
