#ifndef GRAPHICS_STRUCTS_H
#define GRAPHICS_STRUCTS_H

#include <deque>
#include <functional>

#include <vulkan/vulkan.h>

#include "descriptors.h"


namespace Graphics
{

/**
 * @brief Deferred resource cleanup queue for Vulkan objects
 *
 * Stores cleanup operations to be executed later, typically at frame completion.
 * Enables safe destruction of Vulkan resources after GPU work completion.
 */
struct DeletionQueue
{
	std::deque<std::function<void()>> deletors {};

	inline void push_function(std::function<void()> &&function)
	{
		deletors.push_back(function);
	}

	/// @brief Executes all queued cleanup operations
	void flush();
};

/**
 * @brief Per-frame Vulkan resources and synchronization primitives
 *
 * Contains all resources needed for a single frame's rendering work,
 * including command buffers, synchronization objects, and descriptor management.
 */
struct FrameData
{
	/// @brief Command pool for this frame's command buffers
	VkCommandPool commandPool = VK_NULL_HANDLE;

	/// @brief Primary command buffer for this frame's rendering commands
	VkCommandBuffer mainCommandBuffer = VK_NULL_HANDLE;

	/// @brief Semaphore for swapchain image acquisition
	VkSemaphore swapchainSemaphore = VK_NULL_HANDLE;

	/// @brief Semaphore for rendering completion signaling
	VkSemaphore renderSemaphore = VK_NULL_HANDLE;

	/// @brief Fence for CPU-GPU synchronization
	VkFence renderFence = VK_NULL_HANDLE;

	/// @brief Deferred cleanup queue for this frame's resources
	DeletionQueue deletionQueue {};

	/// @brief Descriptor allocator for frame-local descriptors
	DescriptorAllocatorGrowable frameDescriptors {};
};


}


#endif // GRAPHICS_STRUCTS_H
