#ifndef GRAPHICS_ALLOCATEDIMAGE_H
#define GRAPHICS_ALLOCATEDIMAGE_H

#include <vulkan/vulkan.h>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


struct AllocatedImage
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkExtent3D imageExtent = {0, 0, 0};
	VkFormat imageFormat = VK_FORMAT_MAX_ENUM;
};


#endif // GRAPHICS_ALLOCATEDIMAGE_H
