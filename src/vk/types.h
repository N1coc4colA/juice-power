#ifndef TYPES_H
#define TYPES_H

#include <vulkan/vulkan.h>

#include "vma.h"


struct AllocatedImage
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};


#endif // TYPES_H
