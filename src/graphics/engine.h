#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

#include <span>
#include <functional>

#include <vulkan/vulkan.h>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#include "../vk/descriptors.h"
#include "../vk/structs.h"
#include "../vk/types.h"
#include "../vk/allocatedimage.h"


typedef struct SDL_Window SDL_Window;
class Scene;


namespace Graphics
{

constexpr unsigned int FRAME_OVERLAP = 2;


class Engine
{
public:
	/// Getter for the engine instance
	static Engine &get();

	void init();
	void run();
	void cleanup();

	GPUMeshBuffers uploadMesh(const std::span<const uint32_t> &indices, const std::span<const Vertex> &vertices);

	AllocatedImage createImage(const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false);
	AllocatedImage createImage(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false);
	void destroyImage(const AllocatedImage &img);

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&function);

	Scene *scene = nullptr;

protected:
	void initVulkan();
	void initSDL();
	void initVMA();
	void initSwapChain();
	void initCommands();
	void initSyncStructures();
	void initDescriptors();
	void initPipelines();
	void initBackgroundPipelines();
	void initImgui();

	void createSwapChain(uint32_t w, uint32_t h);
	void resizeSwapChain();

	void draw();
	void drawBackground(VkCommandBuffer cmd);
	void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void drawGeometry();

	void destroySwapChain();

	inline FrameData &getCurrentFrame()
	{
		return frames[frameNumber % FRAME_OVERLAP];
	};

	AllocatedBuffer createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer &buffer);

private:
	/* Engine Resources */

	/// Whether the engine has been properly setup.
	bool isInitialized = false;
	/// Current frame ID.
	int frameNumber = 0;
	bool stopRendering = false;
	bool resizeRequested = false;
	VkExtent2D windowExtent = { 1700 , 900 };
	SDL_Window *window = nullptr;
	FrameData frames[FRAME_OVERLAP];

	/* General Resources */

	VmaAllocator allocator = VK_NULL_HANDLE;

	DeletionQueue mainDeletionQueue {};

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice chosenGPU = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = 0;

	/* SwapChain Resources */

	/// Our Vulkan swap chain
	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	/// Image format expected by the windowing system
	VkFormat swapChainImageFormat = VK_FORMAT_MAX_ENUM;
	/// Array of images from the swapchain
	std::vector<VkImage> swapChainImages {};
	/// Array of image-views from the swapchain
	std::vector<VkImageView> swapChainImageViews {};
	VkExtent2D swapChainExtent = {0, 0};

	/* Drawing Resources */

	AllocatedImage drawImage {};
	AllocatedImage depthImage {};
	VkExtent2D drawExtent = {0, 0};
	float renderingScale = 1.f;

	DescriptorAllocatorGrowable globalDescriptorAllocator {};

	VkDescriptorSet drawImageDescriptors = VK_NULL_HANDLE;
	VkDescriptorSetLayout drawImageDescriptorLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout gpuSceneDataDescriptorLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout singleImageDescriptorLayout = VK_NULL_HANDLE;

	VkFence immFence = VK_NULL_HANDLE;
	VkCommandBuffer immCommandBuffer = VK_NULL_HANDLE;
	VkCommandPool immCommandPool = VK_NULL_HANDLE;

	AllocatedImage _errorCheckerboardImage {};

	VkSampler _defaultSamplerLinear = VK_NULL_HANDLE;
	VkSampler _defaultSamplerNearest = VK_NULL_HANDLE;

	EngineStats stats {};

	/* Temporaries */

	VkPipeline gradientPipeline = VK_NULL_HANDLE;
	VkPipelineLayout gradientPipelineLayout = VK_NULL_HANDLE;
};

}


#endif // GRAPHICS_ENGINE_H
