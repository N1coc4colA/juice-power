#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

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
	void run(const std::function<void()> &prepare, const std::function<void()> &update);
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
	inline FrameData &getCurrentFrame()
	{
		return frames[frameNumber % FRAME_OVERLAP];
	};

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
	AllocatedImage createImage(const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false);

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
	AllocatedImage createImage(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false);

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
	AllocatedBuffer createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage);

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

	bool isInitialized = false;
	int frameNumber = 0;
	bool stopRendering = false;
	bool resizeRequested = false;
	float renderScale = 1.f;

	DeletionQueue mainDeletionQueue {};

	/* Windowing */

	VkExtent2D windowExtent = {1700, 900};
	struct SDL_Window *window = nullptr;

	/* Vulkan */

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice chosenGPU = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = 0;
	FrameData frames[FRAME_OVERLAP] {};
	VmaAllocator allocator = VK_NULL_HANDLE;

	/* Swapchain */

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainImageFormat = VK_FORMAT_MAX_ENUM;
	std::vector<VkImage> swapchainImages {};
	std::vector<VkImageView> swapchainImageViews {};
	VkExtent2D swapchainExtent = {0, 0};

	/* Drawing */

	AllocatedImage drawImage {};
	AllocatedImage depthImage {};
	VkExtent2D drawExtent = {0, 0};

	DescriptorAllocatorGrowable globalDescriptorAllocator {};

	VkDescriptorSet drawImageDescriptors = VK_NULL_HANDLE;
	VkDescriptorSetLayout drawImageDescriptorLayout = VK_NULL_HANDLE;

	/* Imaging */

	// Geometry
	VkPipelineLayout meshPipelineLayout = VK_NULL_HANDLE;
	VkPipeline meshPipeline = VK_NULL_HANDLE;
	GPUMeshBuffers meshBuffers {};

	void generateMeshes();

	// Bg
	VkPipeline gradientPipeline = VK_NULL_HANDLE;
	VkPipelineLayout gradientPipelineLayout = VK_NULL_HANDLE;

	// Other
	VkSampler defaultSamplerLinear = VK_NULL_HANDLE;
	VkSampler defaultSamplerNearest = VK_NULL_HANDLE;
	VkDescriptorSetLayout singleImageDescriptorLayout = VK_NULL_HANDLE;

	/* Direct rendering */

	VkFence immFence = VK_NULL_HANDLE;
	VkCommandBuffer immCommandBuffer = VK_NULL_HANDLE;
	VkCommandPool immCommandPool = VK_NULL_HANDLE;

	/* Images */

	AllocatedImage whiteImage {};
	AllocatedImage errorCheckerboardImage {};

	/* Scene */

	GPUSceneData sceneData {};
	VkDescriptorSetLayout gpuSceneDataDescriptorLayout = VK_NULL_HANDLE;
	World::Scene *m_scene = nullptr;

    /* Animation */
    double deltaMS = 0.0;

    friend class ::Loaders::Map;
    friend class Resources;
};


}


#endif // GRAPHICS_ENGINE_H
