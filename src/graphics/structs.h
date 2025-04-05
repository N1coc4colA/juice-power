#ifndef GRAPHICS_STRUCTS_H
#define GRAPHICS_STRUCTS_H

#include <deque>
#include <functional>

#include <vulkan/vulkan.h>

#include "descriptors.h"


namespace Graphics
{

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors {};

	inline void push_function(std::function<void()> &&function)
	{
		deletors.push_back(function);
	}

	void flush();
};

struct FrameData
{
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer mainCommandBuffer = VK_NULL_HANDLE;

	VkSemaphore swapchainSemaphore = VK_NULL_HANDLE,
				renderSemaphore = VK_NULL_HANDLE;
	VkFence renderFence = VK_NULL_HANDLE;

	DeletionQueue deletionQueue {};
	DescriptorAllocatorGrowable frameDescriptors {};
};


}


#endif // GRAPHICS_STRUCTS_H
