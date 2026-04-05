#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

#include <atomic>
#include <chrono>
#include <cstring>
#include <span>

#include <vulkan/vulkan.h>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#include "allocatedimage.h"
#include "descriptors.h"
#include "objectdata.h"
#include "structs.h"
#include "types.h"

#include "src/keywords.h"

struct SDL_Window;

namespace Loaders
{
class Map;
}

namespace World
{
class Scene;
class Chunk;
}


namespace Graphics
{

class Resources;
class Resources2;

/// @brief Number of in-flight frames.
constexpr unsigned int FRAME_OVERLAP = 2;


/**
 * @brief Class responsible of rendering.
 *
 * @warning Any allocation & deletion of resources must be called sync to GPU work.
 *
 **/
class Engine
{
public:
    /// @brief Initial descriptor sets reserved for global allocator.
    static constexpr int initialSetCount = 10;
    /// @brief Initial per-frame descriptor set budget.
    static constexpr int frameInitialSetCount = 1000;
    /// @brief Default quad index count per object.
    static constexpr int standardIndexCount = 6;
    /// @brief Milliseconds-per-second conversion constant.
    static constexpr double msRelSec = 1000.0;
    /// @brief Microseconds-per-millisecond conversion constant.
    static constexpr double usRelMs = 1000.0;
    /// @brief Sleep throttle value for main loop pacing.
    static constexpr int throttleMs = 10;
    /// @brief Default Vulkan wait timeout in nanoseconds.
    static constexpr uint64_t standardVkTimeout = 1000000000;
    /// @brief Long Vulkan wait timeout in nanoseconds.
    static constexpr uint64_t standardInfiniteVkTimeout = 9999999999;
    /// @brief Default window width.
    static constexpr int windowWidth = 1700;
    /// @brief Default window height.
    static constexpr int windowHeight = 900;
    /// @brief Horizontal camera/world offset for orthographic projection.
    static constexpr float orthographicHorizontalOffset = 80.f;
    /// @brief Vertical camera/world offset for orthographic projection.
    static constexpr float orthographicVerticalOffset = 50.f;

    /// @brief Destroys the graphics engine and owned resources.
    ~Engine();

    /// @brief Returns the global engine instance.
    static auto get() -> Engine &;

    /// @brief Inits the engine & related libs
    void init();
    /// @brief Runs the main rendering loop
    void run(const std::function<void()> &prepare, std::atomic<uint64_t> &commands);
    /// @brief Stops the engine, cleans the resources & notifies related libs.
    void cleanup();

    /// @brief Sets the scene used for rendering.
    void setScene(const std::shared_ptr<World::Scene> &scene);

    /**
	 * @brief Uploads mesh data to GPU memory
	 * @param indices Index data span
	 * @param vertices Vertex data span (must match Vertex struct layout)
	 * @return GPUMeshBuffers containing uploaded GPU resources
	 */
    _nodiscard auto uploadMesh(const std::span<const uint32_t> &indices, const std::span<const Vertex> &vertices) -> GPUMeshBuffers;
    /// @brief Uploads line mesh data to GPU memory.
    _nodiscard auto uploadMesh(const std::span<const uint32_t> &indices, const std::span<const LineVertex> &vertices) -> GPULineBuffers;
    /// @brief Uploads animation data to GPU memory.
    _nodiscard auto uploadMesh(const std::span<const AnimationData> &animations) -> GPUAnimationBuffers;

    /// @brief Updates GPU object-data staging used by draw calls.
    void uploadObjectDataForDrawing();
    /// @brief Uploads object data span into GPU storage.
    void uploadObjectData(const std::span<Graphics::ObjectData> &objectData);

    /// @brief Returns current scene.
    _nodiscard inline auto scene() -> std::shared_ptr<World::Scene> { return m_scene; }
    /// @brief Returns current scene (const overload).
    _nodiscard inline auto scene() const -> const std::shared_ptr<World::Scene> { return m_scene; }

    /// @brief Queries maximum image dimension supported by the selected device.
    _nodiscard auto getDeviceMaxImageSize() const -> uint64_t;

protected:
    /// @brief Initializes SDL and creates the window.
    void initSDL();
    /// @brief Deinitializes SDL resources.
    void deinitSDL();
    /// @brief Initializes Vulkan instance/device/swap chain dependencies.
    void initVulkan();
    /// @brief Initializes Vulkan Memory Allocator.
    void initVMA();
    /// @brief Initializes swapchain-dependent resources.
    void initSwapchain();
	/// @brief Initializes command pools and command buffers.
	void initCommands();
	/// @brief Initializes semaphores and fences.
	void initSyncStructures();
	/// @brief Initializes descriptor allocators and layouts.
	void initDescriptors();
	/// @brief Initializes graphics/compute pipelines.
	void initPipelines();
	/// @brief Initializes the mesh rendering pipeline.
	void initMeshPipeline();
	/// @brief Initializes background rendering pipeline(s).
	void initBackgroundPipelines();
	/// @brief Initializes ImGui backends and resources.
	void initImgui();
	/// @brief Initializes default fallback textures and samplers.
	void initDefaultData();
    /// @brief Initializes GPU buffer storing object data.
    void initObjectDataBuffer();

    /// @brief Initializes descriptor sets for loaded images.
    void initImageDescriptors(const uint32_t imageCount);
    /// @brief Releases image descriptor resources.
    void deinitImageDescriptors();

    /// @brief Performs one frame render submission.
    void draw();
    /// @brief Draws background pass.
    void drawBackground(VkCommandBuffer cmd);
    /// @brief Draws ImGui pass to the target image.
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    /// @brief Draws scene geometry.
    void drawGeometry2(VkCommandBuffer cmd);
    // For debugging purposes.
    /// @brief Draws physics debug overlays.
    void drawPhysics2(VkCommandBuffer cmd);
    /// @brief Draws point debug overlays.
    void drawPoints2(VkCommandBuffer cmd);

    /// @brief Creates swapchain objects for the requested extent.
    void createSwapchain(const uint32_t w, const uint32_t h);
    /// @brief Destroys swapchain objects.
    void destroySwapchain();
    /// @brief Recreates swapchain when window size changes.
    void resizeSwapchain();

    //void updateAnimations(World::Scene &scene);
    /// @brief Updates animation state for the current scene.
    void updateAnimations2(const std::shared_ptr<World::Scene> &scene);

private:
    /// @brief Returns frame-local state for the current in-flight frame.
    _nodiscard inline auto getCurrentFrame() -> FrameData & { return m_frames[m_frameNumber % FRAME_OVERLAP]; };
    /// @brief Returns frame-local state for the current in-flight frame (const overload).
    _nodiscard inline auto getCurrentFrame() const -> const FrameData & { return m_frames[m_frameNumber % FRAME_OVERLAP]; };

    /**
	 * @brief Creates an empty GPU image
	 * @param size Image dimensions in pixels (width, height, depth)
	 * @param format Vulkan format
	 * @param usage Combination of VkImageUsageFlagBits
	 * @param mipmapped Whether to allocate full mip chain
	 * @return Allocated image with image view
	 *
	 * @note Always allocates as GPU-only device-local memory
	 * @note Automatically handles depth format aspect flags
	 */
    auto createImage(const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false) -> AllocatedImage;

    /**
	 * @brief Creates and initializes a GPU image with pixel data
	 * @param data Pointer to raw pixel data (must match format/size)
	 * @param size Image dimensions in pixels
	 * @param format Vulkan format for the image
	 * @param usage Combination of VkImageUsageFlagBits
	 * @param mipmapped Whether to generate full mip chain
	 * @return Fully initialized image ready for shader access
	 * @throws Failure on allocation or transfer failure
	 *
	 * @note Automatically handles:
	 *	   - Staging buffer creation/copy
	 *	   - Layout transitions
	 *	   - Mipmap generation (if enabled)
	 */
    auto createImage(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false)
        -> AllocatedImage;

    /**
	 * @brief Destroys image resources
	 * @param img Image to destroy (must not be in use by GPU)
	 *
	 * @note Automatically destroys both image and image view
	 */
    void destroyImage(const AllocatedImage &img);

    /**
	 * @brief Destroys cached image resources and frees associated descriptor set.
	 * @param img Cached image entry to destroy.
	 */
    void destroyImage(const CachedImage &img);

    /**
	 * @brief Creates a GPU buffer
	 * @param allocSize Size in bytes (must be > 0)
	 * @param usage Combination of VkBufferUsageFlagBits
	 * @param memoryUsage VMA memory usage type
	 * @return Fully allocated buffer
	 *
	 * @note Creates with VMA_ALLOCATION_CREATE_MAPPED_BIT by default
	 */
    auto createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage) -> AllocatedBuffer;

    /**
	 * @brief Destroys buffer resources
	 * @param buffer Buffer to destroy (must not be in use by GPU)
	 *
	 * @warning Must ensure GPU work is complete before calling
	 */
    void destroyBuffer(const AllocatedBuffer &buffer);

    /// @brief Generates ASAP exec of a drawing function, synced with GPU.
    void immediateSubmit(const std::function<void(VkCommandBuffer cmd)> &function);

    /* General */

    /// @brief Indicates whether initialization completed successfully.
    bool m_isInitialized = false;
    /// @brief Monotonic frame counter.
    int m_frameNumber = 0;
    /// @brief Set when swapchain resize is required.
    bool m_resizeRequested = false;
    /// @brief Render scale multiplier.
    float m_renderScale = 1.f;

    /// @brief Queue of deferred cleanups executed at shutdown.
    DeletionQueue m_mainDeletionQueue{};

    /* Windowing */

    /// @brief Current window extent.
    VkExtent2D m_windowExtent = {windowWidth, windowHeight};
    /// @brief SDL window handle.
    struct SDL_Window *m_window = nullptr;

    /* Vulkan */

    /// @brief Vulkan instance handle.
    VkInstance m_instance = VK_NULL_HANDLE;
    /// @brief Debug messenger handle.
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    /// @brief Presentation surface handle.
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    /// @brief Logical device handle.
    VkDevice m_device = VK_NULL_HANDLE;
    /// @brief Selected physical GPU.
    VkPhysicalDevice m_chosenGPU = VK_NULL_HANDLE;
    /// @brief Graphics queue handle.
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    /// @brief Graphics queue family index.
    uint32_t m_graphicsQueueFamily = 0;
    /// @brief Per-frame resources.
    std::array<FrameData, FRAME_OVERLAP> m_frames{};
    /// @brief VMA allocator handle.
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    /* Swapchain */

    /// @brief Swapchain handle.
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    /// @brief Swapchain image format.
    VkFormat m_swapchainImageFormat = VK_FORMAT_MAX_ENUM;
    /// @brief Swapchain images.
    std::vector<VkImage> m_swapchainImages{};
    /// @brief Swapchain image views.
    std::vector<VkImageView> m_swapchainImageViews{};
    /// @brief Swapchain extent.
    VkExtent2D m_swapchainExtent = {0, 0};

    /* Drawing */

    /// @brief Offscreen color draw image.
    AllocatedImage m_drawImage{};
    /// @brief Offscreen depth image.
    AllocatedImage m_depthImage{};
    /// @brief Effective draw extent.
    VkExtent2D m_drawExtent = {0, 0};

    /// @brief Global growable descriptor allocator.
    DescriptorAllocatorGrowable m_globalDescriptorAllocator{};

    /// @brief Descriptor set sampling from draw image.
    VkDescriptorSet m_drawImageDescriptors = VK_NULL_HANDLE;
    /// @brief Descriptor layout for draw-image descriptor.
    VkDescriptorSetLayout m_drawImageDescriptorLayout = VK_NULL_HANDLE;

    /* Imaging */

    // Geometry
    /// @brief Pipeline layout for mesh rendering.
    VkPipelineLayout m_meshPipelineLayout = VK_NULL_HANDLE;
    /// @brief Mesh rendering pipeline.
    VkPipeline m_meshPipeline = VK_NULL_HANDLE;

    // Lines, for debugging purposes.
    /// @brief Pipeline layout for line rendering.
    VkPipelineLayout m_linePipelineLayout = VK_NULL_HANDLE;
    /// @brief Line rendering pipeline.
    VkPipeline m_linePipeline = VK_NULL_HANDLE;
    /// @brief Descriptor layout used by line pipeline.
    VkDescriptorSetLayout m_lineDescriptorLayout = VK_NULL_HANDLE;

    // Points, for debugging purposes
    /// @brief Pipeline layout for point rendering.
    VkPipelineLayout m_pointPipelineLayout = VK_NULL_HANDLE;
    /// @brief Point rendering pipeline.
    VkPipeline m_pointPipeline = VK_NULL_HANDLE;
    /// @brief Descriptor layout used by point pipeline.
    VkDescriptorSetLayout m_pointDescriptorLayout = VK_NULL_HANDLE;

    // Bg
    /// @brief Background gradient pipeline.
    VkPipeline m_gradientPipeline = VK_NULL_HANDLE;
    /// @brief Pipeline layout for background gradient pass.
    VkPipelineLayout m_gradientPipelineLayout = VK_NULL_HANDLE;

    // Other
    /// @brief Default linear sampler.
    VkSampler m_defaultSamplerLinear = VK_NULL_HANDLE;
    /// @brief Default nearest sampler.
    VkSampler m_defaultSamplerNearest = VK_NULL_HANDLE;
    /// @brief Descriptor layout used for single-image sampling.
    VkDescriptorSetLayout m_singleImageDescriptorLayout = VK_NULL_HANDLE;

    /* Direct rendering */

    /// @brief Fence for immediate submit helper.
    VkFence m_immFence = VK_NULL_HANDLE;
    /// @brief Command buffer for immediate submit helper.
    VkCommandBuffer m_immCommandBuffer = VK_NULL_HANDLE;
    /// @brief Command pool for immediate submit helper.
    VkCommandPool m_immCommandPool = VK_NULL_HANDLE;

    /// @brief World matrix uploaded in draw push constants.
    glm::mat4 worldMatrix{};

    /* Images */

    /// @brief 1x1 white fallback texture.
    AllocatedImage m_whiteImage{};
    /// @brief Checkerboard error fallback texture.
    AllocatedImage m_errorCheckerboardImage{};

    /* Scene */

    /// @brief Active scene pointer.
    std::shared_ptr<World::Scene> m_scene = nullptr;
    /// @brief Allocator for per-image freeable descriptor sets.
    DescriptorAllocatorFreeable m_imageDescriptorAllocator{};

    /* Animation */

    /// @brief Delta time in milliseconds.
    double m_deltaMS = 0.0;

    /* Stats data */

    /// @brief Number of objects rendered this frame.
    size_t m_objCount = 0;
    /// @brief Number of descriptor/texture switches this frame.
    int m_switchesCount = 0;

    /* Objects */

    /// @brief GPU object-data buffer wrapper.
    GPUObjectDataBuffer m_objectDataBuffer{};

    /* Others */

    /// @brief Singleton storage for loaded engine instance.
    static Engine *loadedEngine;
    /// @brief Timestamp of previous frame for delta computation.
    static decltype(std::chrono::system_clock::now()) prevChrono;

    friend class ::Loaders::Map;
    friend class Resources;
    friend class Resources2;
    friend class DrawingFuncs;
};
}


#endif // GRAPHICS_ENGINE_H
