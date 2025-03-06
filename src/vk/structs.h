#ifndef STRUCTS_H
#define STRUCTS_H

#include <deque>
#include <functional>

#include <vulkan/vulkan.h>


struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	inline void push_function(std::function<void()> &&function)
	{
		deletors.push_back(function);
	}

	void flush();
};

struct FrameData
{
	VkCommandPool _commandPool = VK_NULL_HANDLE;
	VkCommandBuffer _mainCommandBuffer = VK_NULL_HANDLE;

	VkSemaphore _swapchainSemaphore = VK_NULL_HANDLE,
				_renderSemaphore = VK_NULL_HANDLE;
	VkFence _renderFence = VK_NULL_HANDLE;

	DeletionQueue _deletionQueue;
};


#endif // STRUCTS_H
