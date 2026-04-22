#include "engine.h"

#include <gsl/gsl-lite.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include <fmt/printf.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>

#include <ctrack.hpp>

#include "src/config.h"
#include "src/graphics/defines.h"
#include "src/graphics/failure.h"
#include "src/graphics/initializers.h"
#include "src/graphics/pipelinebuilder.h"
#include "src/graphics/utils.h"
#include "src/graphics/vma.h"
#include "src/states.h"
#include "src/world/scene.h"

#ifdef INSTRUMENT
#define LOGFN() \
    { \
        std::cout << __func__ << '\n'; \
    }
#else
#define LOGFN() \
    { \
    }

#endif

namespace {

constexpr bool bUseValidationLayers = true;

} // namespace

namespace Graphics {

Engine *Engine::m_loadedEngine = nullptr;
decltype(std::chrono::system_clock::now()) Engine::m_prevChrono = std::chrono::system_clock::now();

constexpr auto createOrthographicProjection(const float left, const float right, const float bottom, const float top) -> glm::mat4
{
    const float h = top - bottom;
    const float w = right - left;

    constexpr float projectionRange = 2.f;

    auto projection = glm::mat4(1.f);
    projection[0][0] = projectionRange / w;
    projection[1][1] = -projectionRange / h;
    projection[2][2] = 1.f; // Z for layer ordering

    projection[3][0] = (right + left) / w;
    projection[3][1] = (top + bottom) / h;

    return projection;
}

auto Engine::get() -> Engine &
{
	return *m_loadedEngine;
}

Engine::~Engine()
{
    // Just empty the pending events.
    SDL_Event e{};
    while (SDL_PollEvent(&e)) {
    }

    deinitSDL();
}

auto Engine::createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage) -> AllocatedBuffer
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
	vkCheck(vmaCreateBuffer(m_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));
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
	assert(m_loadedEngine == nullptr);
	m_loadedEngine = this;

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

    initObjectDataBuffer();

    //everything went fine apparently
    m_isInitialized = true;
}

void Engine::initSDL()
{
	LOGFN();

	// We initialize SDL and create a window with it.
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        throw Failure(FailureType::SDLInitialisation);
    }

    constexpr SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    m_window = SDL_CreateWindow("Vulkan Engine", static_cast<int>(m_windowExtent.width), static_cast<int>(m_windowExtent.height), windowFlags);

    if (m_window == nullptr) {
        throw Failure(FailureType::SDLWindowCreation);
    }
}

void Engine::deinitSDL()
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();
}

void enumerateDevices(const VkInstance inst)
{
    uint32_t gpuCount = 0;
    vkCheck(vkEnumeratePhysicalDevices(inst, &gpuCount, nullptr));

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkCheck(vkEnumeratePhysicalDevices(inst, &gpuCount, gpus.data()));

    VkPhysicalDeviceProperties properties{};
    for (const auto &gpu : gpus) {
        vkGetPhysicalDeviceProperties(gpu, &properties);

        std::cout << "Available GPU: " << &properties.deviceName[0] << '(' << properties.deviceID << ':' << properties.deviceType << ':'
                  << properties.apiVersion << ':' << properties.driverVersion << ")\n";
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

    const vkb::Instance vkbInstance = instRet.value();

    //store the instance
    m_instance = vkbInstance.instance;
    if (m_instance == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkInstanceCreation);
    }
    //store the debug messenger
    m_debugMessenger = vkbInstance.debug_messenger;
    if (m_debugMessenger == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkDebugMessengerCreation);
    }

    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
        throw Failure(FailureType::VkSurfaceCreation1);
    }

    constexpr VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    //use vkbootstrap to select a GPU.
    //We want a GPU that can write to the SDL m_surface and supports Vulkan 1.3
    const auto physicalDevices = vkb::PhysicalDeviceSelector(vkbInstance)
                                     .set_minimum_version(1, 3) // We run on Vulkan 1.3+
                                     .set_required_features_13(features13)
                                     .set_surface(m_surface)
                                     .select_devices()
                                     .value();

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
    };

    enumerateDevices(m_instance);

    //create the final Vulkan device
    const vkb::Device vkbDevice = vkb::DeviceBuilder(physicalDevices.front()).add_pNext(&bufferDeviceAddressFeatures).build().value();

    // Get the VkDevice handle used in the rest of a Vulkan application
    m_device = vkbDevice.device;
    m_chosenGPU = physicalDevices.front().physical_device;

    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_chosenGPU, &properties);

        std::cout << "Chosen GPU: " << &properties.deviceName[0] << '(' << properties.deviceID << ':' << properties.deviceType << ':'
                  << properties.apiVersion << ':' << properties.driverVersion << ")\n";
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

    vkCheck(vmaCreateAllocator(&allocatorInfo, &m_allocator));
    if (m_allocator == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAInitialisation);
    }

    m_mainDeletionQueue.pushFunction([this]() -> void { vmaDestroyAllocator(m_allocator); });
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

    constexpr VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                                                  | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const VkImageCreateInfo renderingImageInfo = Init::imageCreateInfo(m_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    constexpr VmaAllocationCreateInfo renderingImageAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    //allocate and create the image
    vkCheck(vmaCreateImage(m_allocator, &renderingImageInfo, &renderingImageAllocInfo, &m_drawImage.image, &m_drawImage.allocation, nullptr));
    if (m_drawImage.image == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAImageCreation, "Drawing");
    }

    //build a image-view for the draw image to use for rendering
    const VkImageViewCreateInfo renderingViewInfo = Init::imageViewCreateInfo(m_drawImage.imageFormat, m_drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    vkCheck(vkCreateImageView(m_device, &renderingViewInfo, nullptr, &m_drawImage.imageView));
    if (m_drawImage.imageView == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAImageViewCreation, "Drawing");
    }

    //add to deletion queues
    m_mainDeletionQueue.pushFunction([this]() -> void {
        vkDestroyImageView(m_device, m_drawImage.imageView, nullptr);
        vmaDestroyImage(m_allocator, m_drawImage.image, m_drawImage.allocation);
    });

    // Depth IMG
    m_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    m_depthImage.imageExtent = drawImageExtent;
    constexpr VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    const VkImageCreateInfo depthImageInfo = Init::imageCreateInfo(m_depthImage.imageFormat, depthImageUsages, drawImageExtent);

    //allocate and create the image
    vkCheck(vmaCreateImage(m_allocator, &depthImageInfo, &renderingImageAllocInfo, &m_depthImage.image, &m_depthImage.allocation, nullptr));
    if (m_depthImage.image == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAImageCreation, "Depth");
    }

    //build a image-view for the draw image to use for rendering
    const VkImageViewCreateInfo depthViewInfo = Init::imageViewCreateInfo(m_depthImage.imageFormat, m_depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    vkCheck(vkCreateImageView(m_device, &depthViewInfo, nullptr, &m_depthImage.imageView));
    if (m_depthImage.imageView == VK_NULL_HANDLE) {
		throw Failure(FailureType::VMAImageViewCreation, "Depth");
	}

    m_mainDeletionQueue.pushFunction([this]() -> void {
        vkDestroyImageView(m_device, m_depthImage.imageView, nullptr);
        vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation);
    });
}

void Engine::initCommands()
{
	LOGFN();

	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
    const VkCommandPoolCreateInfo commandPoolInfo = Init::commandPoolCreateInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto &frame : m_frames) {
        vkCheck(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &frame.commandPool));
        if (frame.commandPool == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkCommandPoolCreation, "Frame");
        }

        // allocate the default command buffer that we will use for rendering
        const VkCommandBufferAllocateInfo cmdAllocInfo = Init::commandBufferAllocateInfo(frame.commandPool, 1);

        vkCheck(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &frame.mainCommandBuffer));
        if (frame.mainCommandBuffer == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkCommandBufferCreation, "Frame");
        }
    }

    vkCheck(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_immCommandPool));
    if (m_immCommandPool == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkCommandPoolCreation, "Immediate");
    }

    // allocate the command buffer for immediate submits
    const VkCommandBufferAllocateInfo cmdAllocInfo = Init::commandBufferAllocateInfo(m_immCommandPool, 1);

    vkCheck(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_immCommandBuffer));
	if (m_immCommandBuffer == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkCommandBufferCreation, "Immediate");
	}

    m_mainDeletionQueue.pushFunction([this]() -> void { vkDestroyCommandPool(m_device, m_immCommandPool, nullptr); });
}

void Engine::initSyncStructures()
{
	LOGFN();

	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
    const VkFenceCreateInfo fenceCreateInfo = Init::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = Init::semaphoreCreateInfo();

    for (auto &frame : m_frames) {
        vkCheck(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &frame.renderFence));
        if (frame.renderFence == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkFenceCreation, "Frame");
        }

        vkCheck(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &frame.swapchainSemaphore));
        if (frame.swapchainSemaphore == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkSwapchainCreation, "Frame");
        }
        vkCheck(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &frame.renderSemaphore));
        if (frame.renderSemaphore == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkSemaphoreCreation, "Frame");
        }
    }

    vkCheck(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_immFence));
    if (m_immFence == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkFenceCreation, "Immediate");
    }

    m_mainDeletionQueue.pushFunction([this]() -> void { vkDestroyFence(m_device, m_immFence, nullptr); });
}

void Engine::initDescriptors()
{
	LOGFN();

	//create a descriptor pool that will hold 10 sets with 1 image each
    constexpr std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 2> sizes = {{
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1.f},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 1.f},
    }};

    m_globalDescriptorAllocator.init(m_device, initialSetCount, sizes);

    //make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder{};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        m_drawImageDescriptorLayout = builder.build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
        if (m_drawImageDescriptorLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkDescriptorCreation, "Draw");
        }
    }

    //allocate a descriptor set for our draw image
    m_drawImageDescriptors = m_globalDescriptorAllocator.allocate(m_device, m_drawImageDescriptorLayout);

    DescriptorWriter writer{};
    writer.writeImage(0, m_drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.updateSet(m_device, m_drawImageDescriptors);
    if (m_drawImageDescriptors == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkDescriptorUpdate, "Draw");
    }

    for (auto &frame : m_frames) {
        // create a descriptor pool
        constexpr std::array<DescriptorAllocatorGrowable::PoolSizeRatio, 4> frameSizes = {{
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 3.f},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 3.f},
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3.f},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 4.f},
        }};

        frame.frameDescriptors = DescriptorAllocatorGrowable();
        frame.frameDescriptors.init(m_device, frameInitialSetCount, frameSizes);

        m_mainDeletionQueue.pushFunction([this, &frame]() -> void { frame.frameDescriptors.destroyPools(m_device); });
    }

    {
        DescriptorLayoutBuilder builder{};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        m_singleImageDescriptorLayout = builder.build(m_device, VK_SHADER_STAGE_FRAGMENT_BIT);
        if (m_singleImageDescriptorLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkDescriptorLayoutCreation, "Single");
        }
    }

    {
        DescriptorLayoutBuilder builder{};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        m_lineDescriptorLayout = builder.build(m_device, VK_SHADER_STAGE_VERTEX_BIT);
        if (m_lineDescriptorLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkDescriptorLayoutCreation, "Line");
        }
    }

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    m_mainDeletionQueue.pushFunction([this]() -> void {
        m_globalDescriptorAllocator.destroyPools(m_device);

        vkDestroyDescriptorSetLayout(m_device, m_drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_singleImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_lineDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_pointDescriptorLayout, nullptr);
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
    if (!Utils::loadShaderModule(COMPILED_SHADERS_DIR "/tex_image.frag.spv", m_device, triangleFragShader)) {
        throw Failure(FailureType::FragmentShader, "Triangle");
    }

    VkShaderModule triangleVertexShader = VK_NULL_HANDLE;
    if (!Utils::loadShaderModule(COMPILED_SHADERS_DIR "/colored_triangle_mesh.vert.spv", m_device, triangleVertexShader)) {
        throw Failure(FailureType::VertexShader, "Triangle");
    }

    VkShaderModule lineFragShader = VK_NULL_HANDLE;
    if (!Utils::loadShaderModule(COMPILED_SHADERS_DIR "/line.frag.spv", m_device, lineFragShader)) {
        throw Failure(FailureType::FragmentShader, "Line");
    }

    VkShaderModule lineVertexShader = VK_NULL_HANDLE;
    if (!Utils::loadShaderModule(COMPILED_SHADERS_DIR "/line.vert.spv", m_device, lineVertexShader)) {
        throw Failure(FailureType::VertexShader, "Line");
    }

    VkShaderModule pointFragShader = VK_NULL_HANDLE;
    if (!Utils::loadShaderModule(COMPILED_SHADERS_DIR "/point.frag.spv", m_device, pointFragShader)) {
        throw Failure(FailureType::FragmentShader, "Point");
    }

    VkShaderModule pointVertexShader = VK_NULL_HANDLE;
    if (!Utils::loadShaderModule(COMPILED_SHADERS_DIR "/point.vert.spv", m_device, pointVertexShader)) {
        throw Failure(FailureType::VertexShader, "Point");
    }

    /* Default mesh pipeline */ {
        constexpr VkPushConstantRange bufferRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(GPUDrawPushConstants2),
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = Init::pipelineLayoutCreateInfo();
        pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_singleImageDescriptorLayout;
        pipelineLayoutInfo.setLayoutCount = 1;

        vkCheck(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_meshPipelineLayout));
        if (m_meshPipelineLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkPipelineLayoutCreation);
        }

        PipelineBuilder pipelineBuilder{};
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
        //pipelineBuilder.disableBlending();
        pipelineBuilder.enableBlendingAlphaBlend();
        pipelineBuilder.disableDepthTest();
        //pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        //connect the image format we will draw into, from draw image
        pipelineBuilder.setColorAttachmentFormat(m_drawImage.imageFormat);
        pipelineBuilder.setDepthFormat(m_depthImage.imageFormat);

        //finally build the pipeline
        m_meshPipeline = pipelineBuilder.buildPipeline(m_device);
        if (m_meshPipeline == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkPipelineCreation);
        }
    }

    /* Line pipeline for debugging */ {
        constexpr VkPushConstantRange bufferRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(GPUDrawLinePushConstants),
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = Init::pipelineLayoutCreateInfo();
        pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.setLayoutCount = 0;

        vkCheck(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_linePipelineLayout));
        if (m_linePipelineLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkPipelineLayoutCreation);
        }

        constexpr VkVertexInputBindingDescription binding{
            .binding = 0,
            .stride = sizeof(glm::vec2),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        constexpr VkVertexInputAttributeDescription attribute{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT, // vec2
            .offset = 0,
        };

        PipelineBuilder pipelineBuilder{};

        pipelineBuilder.vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &binding,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &attribute,
        };

        //use the line layout we created
        pipelineBuilder.pipelineLayout = m_linePipelineLayout;
        //connecting the vertex and pixel shaders to the pipeline
        pipelineBuilder.setShaders(lineVertexShader, lineFragShader);
        //it will draw triangles
        pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);
        //filled triangles
        pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
        //no backface culling
        pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        //pipelineBuilder.setCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        //no multisampling
        pipelineBuilder.setMultisamplingNone();
        //no blending
        pipelineBuilder.disableBlending();
        //pipelineBuilder.enableBlendingAlphaBlend();
        //pipelineBuilder.disable_depthtest();
        pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        //connect the image format we will draw into, from draw image
        pipelineBuilder.setColorAttachmentFormat(m_drawImage.imageFormat);
        pipelineBuilder.setDepthFormat(m_depthImage.imageFormat);

        //finally build the pipeline
        m_linePipeline = pipelineBuilder.buildPipeline(m_device);
        if (m_linePipeline == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkPipelineCreation);
        }
    }

    /* Points pipeline for debugging */ {
        constexpr VkPushConstantRange bufferRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(GPUDrawPointPushConstants),
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = Init::pipelineLayoutCreateInfo();
        pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.setLayoutCount = 0;

        vkCheck(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pointPipelineLayout));
        if (m_pointPipelineLayout == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkPipelineLayoutCreation);
        }

        PipelineBuilder pipelineBuilder{};

        pipelineBuilder.vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
        };

        //use the line layout we created
        pipelineBuilder.pipelineLayout = m_pointPipelineLayout;
        //connecting the vertex and pixel shaders to the pipeline
        pipelineBuilder.setShaders(pointVertexShader, pointFragShader);
        //it will draw triangles
        pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        //filled triangles
        pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
        //no backface culling
        pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        //pipelineBuilder.setCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        //no multisampling
        pipelineBuilder.setMultisamplingNone();
        //no blending
        //pipelineBuilder.disableBlending();
        pipelineBuilder.enableBlendingAlphaBlend();
        //pipelineBuilder.disable_depthtest();
        pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        //connect the image format we will draw into, from draw image
        pipelineBuilder.setColorAttachmentFormat(m_drawImage.imageFormat);
        pipelineBuilder.setDepthFormat(m_depthImage.imageFormat);

        //finally build the pipeline
        m_pointPipeline = pipelineBuilder.buildPipeline(m_device);
        if (m_pointPipeline == VK_NULL_HANDLE) {
            throw Failure(FailureType::VkPipelineCreation);
        }
    }

    //clean structures
    vkDestroyShaderModule(m_device, triangleFragShader, nullptr);
    vkDestroyShaderModule(m_device, triangleVertexShader, nullptr);
    vkDestroyShaderModule(m_device, lineFragShader, nullptr);
    vkDestroyShaderModule(m_device, lineVertexShader, nullptr);
    vkDestroyShaderModule(m_device, pointFragShader, nullptr);
    vkDestroyShaderModule(m_device, pointVertexShader, nullptr);

    m_mainDeletionQueue.pushFunction([&]() -> void {
        vkDestroyPipelineLayout(m_device, m_meshPipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_meshPipeline, nullptr);

        vkDestroyPipelineLayout(m_device, m_linePipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_linePipeline, nullptr);

        vkDestroyPipelineLayout(m_device, m_pointPipelineLayout, nullptr);
        vkDestroyPipeline(m_device, m_pointPipeline, nullptr);
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

	vkCheck(vkCreatePipelineLayout(m_device, &computeLayout, nullptr, &m_gradientPipelineLayout));
	if (m_gradientPipelineLayout == VK_NULL_HANDLE) {
		throw Failure(FailureType::VkPipelineLayoutCreation, "Gradient");
	}

	//layout code
	VkShaderModule computeDrawShader = VK_NULL_HANDLE;
    if (!Utils::loadShaderModule(COMPILED_SHADERS_DIR "/gradient.comp.spv", m_device, computeDrawShader)) {
        throw Failure(FailureType::ComputeShader);
    }

    const VkPipelineShaderStageCreateInfo stageinfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeDrawShader,
        .pName = "main",
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .stage = stageinfo,
        .layout = m_gradientPipelineLayout,
    };

    vkCheck(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_gradientPipeline));
    if (m_gradientPipeline == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkPipelineCreation, "Gradient");
    }

    vkDestroyShaderModule(m_device, computeDrawShader, nullptr);

    m_mainDeletionQueue.pushFunction([this]() -> void {
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
    constexpr std::array<VkDescriptorPoolSize, 11> poolSizes = {{{.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = 1000},
                                                                 {.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, .descriptorCount = 1000}}};

    const VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &imguiPool));
    if (imguiPool == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkDescriptorPoolCreation);
    }

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    m_imguiContext = ImGui::CreateContext();
    if (m_imguiContext == nullptr) {
        throw Failure(FailureType::ImguiContext);
    }

    // this initializes imgui for SDL
    if (!ImGui_ImplSDL3_InitForVulkan(m_window)) {
        throw Failure(FailureType::ImguiInitialisation);
    }

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo initInfo {
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

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw Failure(FailureType::ImguiVkInitialisation);
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        throw Failure(FailureType::ImguiFontsInitialisation);
    }

    // add destroying of the imgui-created structures
    m_mainDeletionQueue.pushFunction([this, imguiPool]() -> void {
        ImGui_ImplVulkan_DestroyFontsTexture();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(m_imguiContext);
        vkDestroyDescriptorPool(m_device, imguiPool, nullptr);
    });
}

void Engine::immediateSubmit(const std::function<void(VkCommandBuffer cmd)> &function)
{
	LOGFN();

	assert(m_immFence);
	assert(m_immCommandBuffer);

	vkCheck(vkResetFences(m_device, 1, &m_immFence));
	vkCheck(vkResetCommandBuffer(m_immCommandBuffer, 0));

	const VkCommandBuffer cmd = m_immCommandBuffer;

    const VkCommandBufferBeginInfo cmdBeginInfo = Init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkCheck(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    vkCheck(vkEndCommandBuffer(cmd));

    const VkCommandBufferSubmitInfo cmdInfo = Init::commandBufferSubmitInfo(cmd);
    const VkSubmitInfo2 submit = Init::submitInfo(cmdInfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    vkCheck(vkQueueSubmit2(m_graphicsQueue, 1, &submit, m_immFence));

    vkCheck(vkWaitForFences(m_device, 1, &m_immFence, true, standardInfiniteVkTimeout));
}

void Engine::cleanup()
{
	LOGFN();

	if (m_isInitialized) {
		// We need to wait for the GPU to finish until...
		vkCheck(vkDeviceWaitIdle(m_device));

		// we can destroy the command pools.
		// It may crash the app otherwise.
        for (const auto &frame : m_frames) {
            vkDestroyCommandPool(m_device, frame.commandPool, nullptr);
        }
        for (auto &frame : m_frames) {
            //destroy sync objects
            vkDestroyFence(m_device, frame.renderFence, nullptr);
            vkDestroySemaphore(m_device, frame.renderSemaphore, nullptr);
            vkDestroySemaphore(m_device, frame.swapchainSemaphore, nullptr);

            frame.deletionQueue.flush();
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
        SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
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
    m_loadedEngine = nullptr;
}

void Engine::run(const std::function<void()> &prepare, std::atomic<uint64_t> &commands)
{
	LOGFN();

    m_prevChrono = std::chrono::system_clock::now();

    prepare();

    // main loop
    while (!(commands & CommandStates::Stop)) {
        const auto currentTime = std::chrono::system_clock::now();
        const auto delta = currentTime - m_prevChrono;
        m_deltaMS = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()) / msRelSec;

        //convert to microseconds (integer), and then come back to milliseconds
        const auto frameTime = static_cast<float>(static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(delta).count()) / usRelMs);

        //do not draw if we are minimized
        if (commands & PauseRendering) {
            //throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(throttleMs));
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
        ImGui::Text("Frame time %f ms", frameTime);
        ImGui::Text("Switches ratio %f", static_cast<float>(m_objCount) / static_cast<float>(m_switchesCount));
        ImGui::End();

        ImGui::Render();

        // Request the data to be updated before drawing.
        commands |= PrepareDrawing;
        while (!(commands & (DrawingPrepared | Stop))) {
            // Wait for the submitted work request to be performed & finished.
            // As this loop runs for the rendering, there are no reasons to
            // redraw what's already on the screen. The only reason would be for
            // the animations. So far, this runs smoothly enough.
        }
        // Reset state.
        commands &= ~DrawingPrepared;

        //updateAnimations(*m_scene);
        updateAnimations2(m_scene);
        draw();

        if (m_resizeRequested) {
            resizeSwapchain();
        }

        m_prevChrono = currentTime;
    }
}

void Engine::updateAnimations2(const std::shared_ptr<World::Scene> &scene)
{
    const auto dms = static_cast<float>(m_deltaMS);
    for (auto &obj : scene->objects) {
        obj.animationTime += dms;
    }
}

void Engine::draw()
{
    CTRACK;

    // Naive impl for now
    //const auto pos = m_scene->movings.positions[0];
    const auto pos = m_scene->objects[0].position;
    worldMatrix = createOrthographicProjection(pos.x - orthographicHorizontalOffset,
                                               pos.x + orthographicHorizontalOffset,
                                               pos.y - orthographicVerticalOffset,
                                               pos.y + orthographicVerticalOffset);

    auto &currFrame = getCurrentFrame();

    assert(currFrame.renderFence != VK_NULL_HANDLE);
    // wait until the gpu has finished rendering the last frame. Timeout of 1
    // second
    vkCheck(vkWaitForFences(m_device, 1, &currFrame.renderFence, true, standardVkTimeout));
    currFrame.deletionQueue.flush();
    currFrame.frameDescriptors.clearPools(m_device);

    assert(m_device != VK_NULL_HANDLE);
    assert(m_swapchain != VK_NULL_HANDLE);
    assert(currFrame.swapchainSemaphore != VK_NULL_HANDLE);
    //request image from the swapchain

    uint32_t swapchainImageIndex = -1;
    const VkResult acquireResult
        = vkAcquireNextImageKHR(m_device, m_swapchain, standardVkTimeout, currFrame.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        m_resizeRequested = true;
        return;
    }
    // [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

    vkCheck(vkResetFences(m_device, 1, &currFrame.renderFence));

    //naming it cmd for shorter writing
    const VkCommandBuffer cmd = currFrame.mainCommandBuffer;
    assert(cmd != VK_NULL_HANDLE);

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    vkCheck(vkResetCommandBuffer(cmd, 0));

    m_drawExtent.height = static_cast<uint32_t>(
        static_cast<float>(std::min(m_swapchainExtent.height, m_drawImage.imageExtent.height))
        * m_renderScale);
    m_drawExtent.width = static_cast<uint32_t>(static_cast<float>(std::min(m_swapchainExtent.width, m_drawImage.imageExtent.width)) * m_renderScale);

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    const VkCommandBufferBeginInfo cmdBeginInfo = Init::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkCheck(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    Utils::transitionImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(cmd);

    //transition the draw image and the swapchain image into their correct transfer layouts
    Utils::transitionImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    Utils::transitionImage(cmd, m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    drawGeometry2(cmd);
    //drawPhysics2(cmd);
    //drawPoints2(cmd);

    //transtion the draw image and the swapchain image into their correct transfer layouts
    Utils::transitionImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Utils::transitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    Utils::copyImageToImage(cmd, m_drawImage.image, m_swapchainImages[swapchainImageIndex], m_drawExtent, m_swapchainExtent);

    //draw imgui into the swapchain image
    drawImgui(cmd, m_swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can show it on the screen
    Utils::transitionImage(cmd, m_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    vkCheck(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue.
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished

    const VkCommandBufferSubmitInfo cmdinfo = Init::commandBufferSubmitInfo(cmd);

    const VkSemaphoreSubmitInfo waitInfo = Init::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                       currFrame.swapchainSemaphore);
    const VkSemaphoreSubmitInfo signalInfo = Init::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currFrame.renderSemaphore);

    const VkSubmitInfo2 submit = Init::submitInfo(cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    vkCheck(vkQueueSubmit2(m_graphicsQueue, 1, &submit, currFrame.renderFence));

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    const VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currFrame.renderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &swapchainImageIndex,
    };

    if (vkQueuePresentKHR(m_graphicsQueue, &presentInfo) == VK_ERROR_OUT_OF_DATE_KHR) {
        m_resizeRequested = true;
    }
    // [NOTE] Maybe also check if it has been VK_SUBOPTIMAL_KHR for too long.

    //increase the number of frames drawn
    m_frameNumber++;
}

void Engine::drawBackground(const VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	//make a clear-color from frame number. This will flash with a 120 frame period.
	const float flash = std::abs(std::sin(static_cast<float>(m_frameNumber) / 120.f));
	const VkClearColorValue clearValue { { 0.0f, 0.0f, flash, 1.0f } };

    const VkImageSubresourceRange clearRange = Init::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    //clear image
    vkCmdClearColorImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}

void Engine::drawImgui(const VkCommandBuffer cmd, const VkImageView targetImageView)
{
	assert(cmd != VK_NULL_HANDLE);
	assert(targetImageView != VK_NULL_HANDLE);

    const VkRenderingAttachmentInfo colorAttachment = Init::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Init::renderingInfo(m_swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

/**
 * @brief Internal helper hosting chunk drawing routines.
 */
class DrawingFuncs
{
public:
    /// @brief Draws chunk geometry batches.
    static void drawChunkGeometry2(Engine &engine, const GPUDrawPushConstants2 &pushConstants, const VkCommandBuffer cmd)
        __attribute__((always_inline))
    {
        uint prevImgId = -1;

        for (const auto &refs : engine.m_scene->references) {
            if (const auto currImgId = engine.m_scene->resources->groupedImagesMapping[engine.m_scene->resources->animations[refs.front().animationId].imageId]; currImgId != prevImgId) {
                prevImgId = currImgId;
                ++engine.m_switchesCount;

                // No allocation, no write — just bind the pre-baked set
                auto & [image, descriptorSet] = engine.m_scene->resources->images[currImgId];

                VkDescriptorSet imageSet = descriptorSet;
                // We need to cache the image to the GPU so that it can be used without reuploads.
                if (imageSet == VK_NULL_HANDLE) [[unlikely]] {
                    imageSet = engine.m_imageDescriptorAllocator.allocate(engine.m_device, engine.m_singleImageDescriptorLayout);
                    //imageSet = engine.getCurrentFrame().frameDescriptors.allocate(engine.m_device, engine.m_singleImageDescriptorLayout);

                    DescriptorWriter writer{};
                    writer.writeImage(0,
                                      image.imageView,
                                      engine.m_defaultSamplerNearest,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                    writer.updateSet(engine.m_device, imageSet);
                    descriptorSet = imageSet;
                }

                assert(imageSet != VK_NULL_HANDLE);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine.m_meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);
            }

            vkCmdPushConstants(cmd, engine.m_meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants2), &pushConstants);
            vkCmdBindIndexBuffer(cmd, engine.m_scene->resources->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cmd, Engine::standardIndexCount, refs.size(), 0, 0, static_cast<uint32_t>(engine.m_objCount));

            engine.m_objCount += refs.size();
        }
    }

    /// @brief Draws chunk border/physics debug lines.
    static void drawChunkPhysics2(const Engine &engine, GPUDrawLinePushConstants &pushConstants, const VkCommandBuffer cmd)
        __attribute__((always_inline))
    {
        for (const auto &refs : engine.m_scene->references) {
            const auto currImgId = engine.m_scene->resources->groupedImagesMapping[engine.m_scene->resources->animations[refs.front().animationId].imageId];

            for (const auto &obj : refs) {
                pushConstants.color = engine.m_scene->entities.at<Entity::PhysicsObjectState>(obj.objId).hasCollision ? glm::vec3(1.f, 0.f, 0.f)
                                                                                                                      : glm::vec3(0.f, 1.f, 0.f);
                pushConstants.worldMatrix = glm::translate(engine.worldMatrix, glm::vec3(obj.position.x, obj.position.y, 0.0f));

                vkCmdPushConstants(cmd, engine.m_linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawLinePushConstants), &pushConstants);
                vkCmdBindIndexBuffer(cmd, engine.m_scene->resources->linesBuffer.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(cmd,
                                 static_cast<uint32_t>(engine.m_scene->resources->borders[currImgId].size()),
                                 1,
                                 engine.m_scene->resources->borderOffsets[currImgId],
                                 0,
                                 0);
            }
        }
    }

    /// @brief Draws chunk debug points.
    static void drawChunkPoints2(const Engine &engine, GPUDrawPointPushConstants &pushConstants, const VkCommandBuffer cmd)
        __attribute__((always_inline))
    {
        for (const auto &refs : engine.m_scene->references) {
            for (const auto obj : refs) {
                glm::vec4 worldPos = engine.worldMatrix * glm::vec4(obj.position.x, obj.position.y, 0.0f, 1.0f);
                worldPos /= worldPos.w;

                pushConstants.pos = glm::vec2(worldPos.x, worldPos.y);
                pushConstants.color = engine.m_scene->entities.at<Entity::PhysicsObjectState>(obj.objId).hasCollision
                                          ? glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
                                          : glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

                vkCmdPushConstants(cmd, engine.m_pointPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPointPushConstants), &pushConstants);

                vkCmdDraw(cmd, 1, 1, 0, 0);
            }
        }
    }
};

void Engine::drawGeometry2(const VkCommandBuffer cmd)
{
    assert(cmd != VK_NULL_HANDLE);

    if (!m_scene) {
        return;
    }

    //begin a render pass  connected to our draw image
    const VkRenderingAttachmentInfo colorAttachment = Init::attachmentInfo(m_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = Init::depthAttachmentInfo(m_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    const VkRenderingInfo renderInfo = Init::renderingInfo(m_windowExtent, &colorAttachment, &depthAttachment);

    //set dynamic viewport and scissor
    const VkViewport viewport{
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

    m_objCount = 0;
    m_switchesCount = 0;

    uploadObjectDataForDrawing();

    const GPUDrawPushConstants2 pushConstants{
        .worldMatrix = worldMatrix,
        .vertexBuffer = m_scene->resources->meshBuffers.vertexBufferAddress,
        .animationBuffer = m_scene->resources->animationsBuffer.animationBufferAddress,
        .objectsBuffer = m_objectDataBuffer.deviceAddress,
    };

    // [NOTE] Must match the ordering of uploadObjectDataForDrawing! Otherwise, the
    // instance offsets will be wrong, and elements will not be drawn properly.
    DrawingFuncs::drawChunkGeometry2(*this, pushConstants, cmd);

    vkCmdEndRendering(cmd);
}

void Engine::drawPhysics2(const VkCommandBuffer cmd)
{
    assert(cmd != VK_NULL_HANDLE);

    if (!m_scene) {
        return;
    }

    //begin a render pass  connected to our draw image
    const VkRenderingAttachmentInfo colorAttachment = Init::attachmentInfo(m_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = Init::depthAttachmentInfo(m_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    const VkRenderingInfo renderInfo = Init::renderingInfo(m_windowExtent, &colorAttachment, &depthAttachment);

    //set dynamic viewport and scissor
    const VkViewport viewport{
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    GPUDrawLinePushConstants pushConstants{
        .worldMatrix = worldMatrix,
        .vertexBuffer = m_scene->resources->linesBuffer.vertexBufferAddress,
    };

    DrawingFuncs::drawChunkPhysics2(*this, pushConstants, cmd);

    vkCmdEndRendering(cmd);
}

void Engine::drawPoints2(VkCommandBuffer cmd)
{
    assert(cmd != VK_NULL_HANDLE);

    if (!m_scene) {
        return;
    }

    //begin a render pass  connected to our draw image
    const VkRenderingAttachmentInfo colorAttachment = Init::attachmentInfo(m_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const VkRenderingAttachmentInfo depthAttachment = Init::depthAttachmentInfo(m_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    const VkRenderingInfo renderInfo = Init::renderingInfo(m_windowExtent, &colorAttachment, &depthAttachment);

    //set dynamic viewport and scissor
    const VkViewport viewport{
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pointPipeline);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    GPUDrawPointPushConstants pushConstants{};

    DrawingFuncs::drawChunkPoints2(*this, pushConstants, cmd);

    vkCmdEndRendering(cmd);
}

auto Engine::uploadMesh(const std::span<const uint32_t> &indices, const std::span<const Vertex> &vertices) -> GPUMeshBuffers
{
    LOGFN();

    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface = {
        //create index buffer
        .indexBuffer = createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY),
        //create vertex buffer
        .vertexBuffer = createBuffer(vertexBufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY),
    };

    //find the adress of the vertex buffer
    const VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_device, &deviceAdressInfo);

    const AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = getMappedData(staging.allocation);
    if (data == nullptr) {
        throw Failure(FailureType::MappedAccess);
    }

    // copy vertex buffer
    std::memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    std::memcpy(&static_cast<std::byte *>(data)[vertexBufferSize], indices.data(), indexBufferSize);

    immediateSubmit([&](const VkCommandBuffer cmd) -> void {
        const VkBufferCopy vertexCopy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = vertexBufferSize,
        };

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        const VkBufferCopy indexCopy{
            .srcOffset = vertexBufferSize,
            .dstOffset = 0,
            .size = indexBufferSize,
        };

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroyBuffer(staging);

    return newSurface;
}

void Engine::uploadObjectDataForDrawing()
{
    if (m_scene->entities.empty()) {
        return;
    }

    const size_t elementsCount = m_scene->entities.size();

    assert(elementsCount < Config::maximumObjectCount);

    const size_t dataSize = elementsCount * sizeof(ObjectData);

    // Create a staging buffer
    const AllocatedBuffer staging = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Copy data to staging buffer
    size_t offset = 0;
    for (const auto &ref : m_scene->references) {
        const auto s = ref.size() * sizeof(ObjectData);
        std::memcpy(&static_cast<std::byte *>(staging.info.pMappedData)[offset], ref.data(), s);
        offset += s;
    }

    // Submit a copy command
    immediateSubmit([&](const VkCommandBuffer cmd) -> void {
        const VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = dataSize};
        vkCmdCopyBuffer(cmd, staging.buffer, m_objectDataBuffer.buffer.buffer, 1, &copy);
    });

    destroyBuffer(staging);
}

void Engine::uploadObjectData(const std::span<ObjectData> &objectData)
{
    if (objectData.empty()) {
        return;
    }

    const size_t dataSize = objectData.size() * sizeof(ObjectData);

    // Create a staging buffer
    const AllocatedBuffer staging = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // Copy data to staging buffer
    std::memcpy(staging.info.pMappedData, objectData.data(), dataSize);

    // Submit a copy command
    immediateSubmit([&](const VkCommandBuffer cmd) -> void {
        const VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = dataSize};
        vkCmdCopyBuffer(cmd, staging.buffer, m_objectDataBuffer.buffer.buffer, 1, &copy);
    });

    destroyBuffer(staging);
}

auto Engine::getDeviceMaxImageSize() const -> uint64_t
{
    assert(m_chosenGPU != VK_NULL_HANDLE);

    VkImageFormatProperties props{};
    vkGetPhysicalDeviceImageFormatProperties(m_chosenGPU,
                                             VK_FORMAT_R8G8B8A8_UNORM,
                                             VK_IMAGE_TYPE_2D,
                                             VK_IMAGE_TILING_OPTIMAL,
                                             VK_IMAGE_USAGE_SAMPLED_BIT,
                                             VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
                                             &props);

    return props.maxResourceSize;
}

auto Engine::uploadMesh(const std::span<const AnimationData> &animations) -> GPUAnimationBuffers
{
    LOGFN();

    for (const auto &anim : animations) {
        for (int i = 0; i < 4; i++) {
            assert(anim.imageInfo[i] != -1);
        }
    }

    const size_t animationBufferSize = sizeof(AnimationData) * animations.size();

    GPUAnimationBuffers newSurface = {
        .animationBuffer = createBuffer(animationBufferSize,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VMA_MEMORY_USAGE_GPU_ONLY),
    };

    const VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                     .buffer = newSurface.animationBuffer.buffer};
    newSurface.animationBufferAddress = vkGetBufferDeviceAddress(m_device, &deviceAdressInfo);

    const AllocatedBuffer staging = createBuffer(animationBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = getMappedData(staging.allocation);
    if (data == nullptr) {
        throw Failure(FailureType::MappedAccess);
    }

    std::memcpy(data, animations.data(), animationBufferSize);

    immediateSubmit([&](const VkCommandBuffer cmd) -> void {
        const VkBufferCopy vertexCopy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = animationBufferSize,
        };

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.animationBuffer.buffer, 1, &vertexCopy);
    });

    destroyBuffer(staging);

    return newSurface;
}

auto Engine::uploadMesh(const std::span<const uint32_t> &indices, const std::span<const LineVertex> &vertices) -> GPULineBuffers
{
    LOGFN();

    const size_t vertexBufferSize = vertices.size() * sizeof(LineVertex);

    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPULineBuffers newSurface = {
        //create index buffer
        .indexBuffer = createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY),
        //create vertex buffer
        .vertexBuffer = createBuffer(vertexBufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     VMA_MEMORY_USAGE_GPU_ONLY),
    };

    //find the address of the vertex buffer
    const VkBufferDeviceAddressInfo deviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_device, &deviceAddressInfo);

    const AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = getMappedData(staging.allocation);
    if (data == nullptr) {
        throw Failure(FailureType::MappedAccess);
    }

    // copy vertex buffer
    std::memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    std::memcpy(&static_cast<std::byte *>(data)[vertexBufferSize], indices.data(), indexBufferSize);

    immediateSubmit([&](const VkCommandBuffer cmd) -> void {
        const VkBufferCopy vertexCopy{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = vertexBufferSize,
        };

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        const VkBufferCopy indexCopy{
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

    //3 default textures, white, grey, black. 1 pixel each
    const uint32_t white = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
    const uint32_t black = glm::packUnorm4x8(glm::vec4(1.f, 1.f, 1.f, 1.f));
    m_whiteImage = createImage(&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    //checkerboard image
    const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1.f, 0.f, 1.f, 1.f));
    constexpr int imgSize = 16;
    std::array<uint32_t, static_cast<size_t>(imgSize * imgSize)> pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < imgSize; ++x) {
        for (int y = 0; y < imgSize; ++y) {
            pixels[y * imgSize + x] = (x % 2) ^ (y % 2) ? magenta : black;
        }
    }
    m_errorCheckerboardImage = createImage(pixels.data(), VkExtent3D{imgSize, imgSize, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampler = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
    };

    vkCheck(vkCreateSampler(m_device, &sampler, nullptr, &m_defaultSamplerNearest));
    if (m_defaultSamplerNearest == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkSamplerCreation, "Nearest");
    }

    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    vkCheck(vkCreateSampler(m_device, &sampler, nullptr, &m_defaultSamplerLinear));
    if (m_defaultSamplerLinear == VK_NULL_HANDLE) {
        throw Failure(FailureType::VkSamplerCreation, "Linear");
    }

    m_mainDeletionQueue.pushFunction([this]() -> void {
        vkDestroySampler(m_device, m_defaultSamplerNearest, nullptr);
        vkDestroySampler(m_device, m_defaultSamplerLinear, nullptr);

        destroyImage(m_whiteImage);
        destroyImage(m_errorCheckerboardImage);
    });
}

void Engine::initObjectDataBuffer()
{
    LOGFN();

    // Allocate a buffer large enough for max objects per frame
    // Adjust the size based on your needs

    constexpr size_t buffer_size = Config::maxObjectsPerFrame * sizeof(ObjectData);

    m_objectDataBuffer.buffer = createBuffer(buffer_size,
                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                 | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                             VMA_MEMORY_USAGE_GPU_ONLY);

    const VkBufferDeviceAddressInfo deviceAddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                      .buffer = m_objectDataBuffer.buffer.buffer};

    m_objectDataBuffer.deviceAddress = vkGetBufferDeviceAddress(m_device, &deviceAddressInfo);

    m_mainDeletionQueue.pushFunction([this]() -> void { destroyBuffer(m_objectDataBuffer.buffer); });
}

void Engine::initImageDescriptors(const uint32_t imageCount)
{
    m_imageDescriptorAllocator.init(m_device, imageCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

void Engine::deinitImageDescriptors()
{
    m_imageDescriptorAllocator.destroyPool(m_device);
}

void Engine::createSwapchain(const uint32_t width, const uint32_t height)
{
    LOGFN();

    vkb::SwapchainBuilder swapchain_builder{
        m_chosenGPU,
        m_device,
        m_surface,
    };

    m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkb_swapchain = swapchain_builder.use_default_format_selection()
                                       .set_desired_format(
                                           VkSurfaceFormatKHR{.format = m_swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                                       //use vsync present mode
                                       .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                       .set_desired_extent(width, height)
                                       .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                       .build()
                                       .value();

    //store swap chain and its related images
    m_swapchain = vkb_swapchain.swapchain;
    m_swapchainImages = vkb_swapchain.get_images().value();
    m_swapchainImageViews = vkb_swapchain.get_image_views().value();
    m_swapchainExtent = vkb_swapchain.extent;

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

    // destroy swap chain resources
    for (const auto &iv : m_swapchainImageViews) {
        vkDestroyImageView(m_device, iv, nullptr);
    }
}

void Engine::resizeSwapchain()
{
    LOGFN();

    vkCheck(vkDeviceWaitIdle(m_device));

    destroySwapchain();

    int w = -1, h = -1;
    SDL_GetWindowSize(m_window, &w, &h);
    m_windowExtent.width = w;
    m_windowExtent.height = h;

    createSwapchain(m_windowExtent.width, m_windowExtent.height);

    m_resizeRequested = false;
}

auto Engine::createImage(const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped) -> AllocatedImage
{
    LOGFN();

    assert(format != VK_FORMAT_MAX_ENUM);
    assert(usage != VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM);

    AllocatedImage newImage{
        .imageExtent = size,
        .imageFormat = format,
    };

    VkImageCreateInfo imgInfo = Init::imageCreateInfo(format, usage, size);
    if (mipmapped) {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate images on dedicated GPU memory
    constexpr VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    // allocate and create the image
    vkCheck(vmaCreateImage(m_allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));
    if (newImage.image == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAImageCreation);
    }

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    const VkImageAspectFlags aspectFlag = format != VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;

    // build a image-view for the image
    VkImageViewCreateInfo viewInfo = Init::imageViewCreateInfo(format, newImage.image, aspectFlag);
    viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

    vkCheck(vkCreateImageView(m_device, &viewInfo, nullptr, &newImage.imageView));
    if (newImage.imageView == VK_NULL_HANDLE) {
        throw Failure(FailureType::VMAImageViewCreation);
    }

    return newImage;
}

auto Engine::createImage(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped)
    -> AllocatedImage
{
    LOGFN();

    assert(data != nullptr);
    assert(format != VK_FORMAT_MAX_ENUM);
    assert(usage != VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM);

    const size_t dataSize = static_cast<size_t>(size.depth) * static_cast<size_t>(size.width) * static_cast<size_t>(size.height) * 4;
    const AllocatedBuffer uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    std::memcpy(uploadBuffer.info.pMappedData, data, dataSize);

    const AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    immediateSubmit([&](const VkCommandBuffer cmd) -> void {
        Utils::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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
        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipmapped) {
            Utils::generateMipmaps(cmd, newImage.image, VkExtent2D{newImage.imageExtent.width, newImage.imageExtent.height});
        } else {
            Utils::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroyBuffer(uploadBuffer);

    return newImage;
}

void Engine::destroyImage(const AllocatedImage &img)
{
    LOGFN();

    vkDestroyImageView(m_device, img.imageView, nullptr);
    vmaDestroyImage(m_allocator, img.image, img.allocation);
}

void Engine::destroyImage(const CachedImage &img)
{
    LOGFN();

    if (img.descriptorSet != VK_NULL_HANDLE) {
        m_imageDescriptorAllocator.free(m_device, img.descriptorSet);
    }

    destroyImage(img.image);
}

void Engine::setScene(const std::shared_ptr<World::Scene> &scene)
{
    m_scene = scene;
}

} // namespace Graphics
