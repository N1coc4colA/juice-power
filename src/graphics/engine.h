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
#include "structs.h"
#include "types.h"


struct SDL_Window;

namespace Loaders
{
class Map;
}

namespace World
{
class Scene;
}


namespace Graphics
{

class Resources;

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
	/// @brief Returns the global engine instance.
	static Engine &get();

	/// @brief Inits the engine & related libs
	void init();
	/// @brief Runs the main rendering loop
    void run(const std::function<void()> &prepare, std::atomic<uint64_t> *commands);
    /// @brief Stops the engine, cleans the resources & notifies related libs.
    void cleanup();

	void setScene(World::Scene &scene);

	/**
	 * @brief Uploads mesh data to GPU memory
	 * @param indices Index data span
	 * @param vertices Vertex data span (must match Vertex struct layout)
	 * @return GPUMeshBuffers containing uploaded GPU resources
	 */
	GPUMeshBuffers uploadMesh(const std::span<const uint32_t> &indices, const std::span<const Vertex> &vertices);

protected:
	void initSDL();
	void initVulkan();
	void initVMA();
	void initSwapchain();
	void initCommands();
	void initSyncStructures();
	void initDescriptors();
	void initPipelines();
	void initMeshPipeline();
	void initBackgroundPipelines();
	void initImgui();
	void initDefaultData();

	void draw();
	void drawBackground(VkCommandBuffer cmd);
	void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void drawGeometry(VkCommandBuffer cmd);

	void createSwapchain(const uint32_t w, const uint32_t h);
	void destroySwapchain();
	void resizeSwapchain();

    void updateAnimations(World::Scene &scene);

private:
    inline FrameData &getCurrentFrame() { return m_frames[m_frameNumber % FRAME_OVERLAP]; };

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
    AllocatedImage createImage(const VkExtent3D &size,
                               const VkFormat format,
                               const VkImageUsageFlags usage,
                               const bool mipmapped = false);

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
    AllocatedImage createImage(const void *data,
                               const VkExtent3D &size,
                               const VkFormat format,
                               const VkImageUsageFlags usage,
                               const bool mipmapped = false);

    /**
	 * @brief Destroys image resources
	 * @param img Image to destroy (must not be in use by GPU)
	 *
	 * @note Automatically destroys both image and image view
	 */
    void destroyImage(const AllocatedImage &img);

    /**
	 * @brief Creates a GPU buffer
	 * @param allocSize Size in bytes (must be > 0)
	 * @param usage Combination of VkBufferUsageFlagBits
	 * @param memoryUsage VMA memory usage type
	 * @return Fully allocated buffer
	 *
	 * @note Creates with VMA_ALLOCATION_CREATE_MAPPED_BIT by default
	 */
    AllocatedBuffer createBuffer(const size_t allocSize,
                                 const VkBufferUsageFlags usage,
                                 const VmaMemoryUsage memoryUsage);

    /**
	 * @brief Destroys buffer resources
	 * @param buffer Buffer to destroy (must not be in use by GPU)
	 *
	 * @warning Must ensure GPU work is complete before calling
	 */
    void destroyBuffer(const AllocatedBuffer &buffer);

    /// @brief Generates ASAP exec of a drawing function, synced with GPU.
	void immediate_submit(const std::function<void(VkCommandBuffer cmd)> &function);

	/* General */

    bool m_isInitialized = false;
    int m_frameNumber = 0;
    bool m_resizeRequested = false;
    float m_renderScale = 1.f;

    DeletionQueue m_mainDeletionQueue{};

    /* Windowing */

    VkExtent2D m_windowExtent = {1700, 900};
    struct SDL_Window *m_window = nullptr;

    /* Vulkan */

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_chosenGPU = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    FrameData m_frames[FRAME_OVERLAP]{};
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    /* Swapchain */

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainImageFormat = VK_FORMAT_MAX_ENUM;
    std::vector<VkImage> m_swapchainImages{};
    std::vector<VkImageView> m_swapchainImageViews{};
    VkExtent2D m_swapchainExtent = {0, 0};

    /* Drawing */

    AllocatedImage m_drawImage{};
    AllocatedImage m_depthImage{};
    VkExtent2D m_drawExtent = {0, 0};

    DescriptorAllocatorGrowable m_globalDescriptorAllocator{};

    VkDescriptorSet m_drawImageDescriptors = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_drawImageDescriptorLayout = VK_NULL_HANDLE;

    /* Imaging */

    // Geometry
    VkPipelineLayout m_meshPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_meshPipeline = VK_NULL_HANDLE;
    GPUMeshBuffers m_meshBuffers{};

    void generateMeshes();

    // Bg
    VkPipeline m_gradientPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_gradientPipelineLayout = VK_NULL_HANDLE;

    // Other
    VkSampler m_defaultSamplerLinear = VK_NULL_HANDLE;
    VkSampler m_defaultSamplerNearest = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_singleImageDescriptorLayout = VK_NULL_HANDLE;

    /* Direct rendering */

    VkFence m_immFence = VK_NULL_HANDLE;
    VkCommandBuffer m_immCommandBuffer = VK_NULL_HANDLE;
    VkCommandPool m_immCommandPool = VK_NULL_HANDLE;

    /* Images */

    AllocatedImage m_whiteImage{};
    AllocatedImage m_errorCheckerboardImage{};

    /* Scene */

    GPUSceneData m_sceneData{};
    VkDescriptorSetLayout m_gpuSceneDataDescriptorLayout = VK_NULL_HANDLE;
    World::Scene *m_scene = nullptr;

    /* Animation */
    double m_deltaMS = 0.0;

    static Engine *loadedEngine;
    static decltype(std::chrono::system_clock::now()) prevChrono;

    friend class ::Loaders::Map;
    friend class Resources;
};


}


#endif // GRAPHICS_ENGINE_H
