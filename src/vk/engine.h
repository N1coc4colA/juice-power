#ifndef ENGINE_H
#define ENGINE_H

#include <cstring>
#include <span>

#include <vulkan/vulkan.h>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#include "descriptors.h"
#include "structs.h"
#include "types.h"


#ifndef NDEBUG
//#define VMA_USE_DEBUG_LOG
#endif


constexpr unsigned int FRAME_OVERLAP = 2;


class Engine
{
public:
	/// Getter for the engine instance
	static Engine &get();

	/// Whether the engine has been properly setup.
	bool _isInitialized = false;
	/// Current frame ID.
	int _frameNumber = 0;
	///
	bool stop_rendering = false;

	/// Window size
	VkExtent2D _windowExtent = { 1700 , 900 };

	/// Handle to the SDL window
	struct SDL_Window *_window = nullptr;

	FrameData _frames[FRAME_OVERLAP];

	inline FrameData &get_current_frame()
	{
		return _frames[_frameNumber % FRAME_OVERLAP];
	};

	VmaAllocator _allocator = VK_NULL_HANDLE;

	DeletionQueue _mainDeletionQueue;

	VkInstance _instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;
	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	VkDevice _device = VK_NULL_HANDLE;
	VkPhysicalDevice _chosenGPU = VK_NULL_HANDLE;
	VkQueue _graphicsQueue = VK_NULL_HANDLE;
	uint32_t _graphicsQueueFamily = 0;

	/// Our Vulkan swap chain
	VkSwapchainKHR _swapchain = VK_NULL_HANDLE;

	/// Image format expected by the windowing system
	VkFormat _swapchainImageFormat;

	/// Array of images from the swapchain
	std::vector<VkImage> _swapchainImages;

	/// Array of image-views from the swapchain
	std::vector<VkImageView> _swapchainImageViews;

	//draw resources
	AllocatedImage _drawImage;
	VkExtent2D _swapchainExtent;
	VkExtent2D _drawExtent;

	DescriptorAllocator globalDescriptorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{0};

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;

	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	GPUMeshBuffers rectangle;


	AllocatedBuffer create_buffer(const size_t allocSize, const VkBufferUsageFlags usage, const VmaMemoryUsage memoryUsage);

	void destroy_buffer(const AllocatedBuffer& buffer);

	/// Initializes everything in the engine
	void init();

	/// Initializes the SDL & a window
	void init_sdl();

	/// Inits Vulkan stuff
	void init_vulkan();

	/// Init VMA stuff
	void init_vma();

	/// Inits our swap chain
	void init_swapchain();

	/// Inits Vk Vk*Command*
	void init_commands();

	/// Inits sync elements for drawing
	void init_sync_structures();

	void init_descriptors();

	void init_pipelines();

	void init_mesh_pipeline();

	void init_background_pipelines();

	void init_imgui();

	void init_triangle_pipeline();

	void init_default_data();

	/// Shuts down the engine
	void cleanup();

	/// Draw loop
	void draw();

	/// Draws... yes, the BG.
	void draw_background(VkCommandBuffer cmd);

	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	void draw_pc(VkCommandBuffer cmd);

	void draw_geometry(VkCommandBuffer cmd);


	GPUMeshBuffers uploadMesh(const std::span<const uint32_t> &indices, const std::span<const Vertex> &vertices);


	/// Run main loop
	void run();

	void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);
};


#endif // ENGINE_H
