#ifndef GRAPHICS_ENGINE_H
#define GRAPHICS_ENGINE_H

#include <cstring>
#include <span>

#include <vulkan/vulkan.h>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#include "src/vk/descriptors.h"
#include "src/vk/loader.h"
#include "src/vk/structs.h"
#include "src/vk/types.h"
#include "src/vk/renderobject.h"


class Scene;
struct SDL_Window;


namespace Graphics
{

constexpr unsigned int FRAME_OVERLAP = 2;

class Engine
{
public:
	static Engine &get();

	Scene *scene = nullptr;

	void init();
	void run();
	void cleanup();

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

	void createSwapchain(uint32_t w, uint32_t h);
	void destroySwapchain();
	void resizeSwapchain();

private:
	inline FrameData &getCurrentFrame()
	{
		return frames[frameNumber % FRAME_OVERLAP];
	};

	AllocatedImage createImage(const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false);
	AllocatedImage createImage(const void *data, const VkExtent3D &size, const VkFormat format, const VkImageUsageFlags usage, const bool mipmapped = false);
	void destroyImage(const AllocatedImage &img);

	AllocatedBuffer createBuffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);

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
	FrameData frames[FRAME_OVERLAP];
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
	VkDescriptorSetLayout drawImageDescriptorLayout;

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

	MaterialInstance defaultData {};
	GLTFMetallic_Roughness metalRoughMaterial {};

	/* Direct rendering */

	VkFence immFence = VK_NULL_HANDLE;
	VkCommandBuffer immCommandBuffer = VK_NULL_HANDLE;
	VkCommandPool immCommandPool = VK_NULL_HANDLE;

	/* Images */

	AllocatedImage whiteImage {};
	AllocatedImage blackImage {};
	AllocatedImage greyImage {};
	AllocatedImage errorCheckerboardImage {};

	/* Scene */

	GPUSceneData sceneData {};
	VkDescriptorSetLayout gpuSceneDataDescriptorLayout = VK_NULL_HANDLE;

	friend struct ::GLTFMetallic_Roughness;
};

}


#endif // GRAPHICS_ENGINE_H
