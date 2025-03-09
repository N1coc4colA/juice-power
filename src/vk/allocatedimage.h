#ifndef ALLOCATEDIMAGE_H
#define ALLOCATEDIMAGE_H

#include <vulkan/vulkan.h>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


struct AllocatedImage
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};


#endif // ALLOCATEDIMAGE_H
