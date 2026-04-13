#ifndef JP_GRAPHICS_ALLOCATEDIMAGE_H
#define JP_GRAPHICS_ALLOCATEDIMAGE_H

#include <vulkan/vulkan.h>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


namespace Graphics
{

/**
 * @brief Represents a Vulkan image with memory allocation and view information
 *
 * Combines Vulkan image handles with their associated memory allocation (via VMA)
 * and view information for convenient management. This is a complete package for
 * working with images in Vulkan.
 */
struct AllocatedImage
{
	/// @brief Vulkan image handle
	/// @note This is the raw VkImage object. Mem. management is handled by VMA.
	VkImage image = VK_NULL_HANDLE;

	/// @brief Image view for accessing the image in shaders
	/// @note Required for most operations except memory management.
	VkImageView imageView = VK_NULL_HANDLE;

	/// @brief Memory allocation handle from (VMA)
	/// @note Manages both memory allocation and lifetime of the VkImage.
	VmaAllocation allocation = VK_NULL_HANDLE;

	/// @brief Dimensions of the image in pixels
	/// @note For 2D images, depth is the layer.
	VkExtent3D imageExtent {.width = 0, .height = 0, .depth = 0};

	/// @brief Pixel format of the image
	/// @note Must match the format used when creating the image.
	VkFormat imageFormat = VK_FORMAT_MAX_ENUM;
};

/**
 * @brief Represents an images that stays on the GPU between calls.
 * For performance reasons, leaving the image on the GPU instead of re-uploading
 * it every time is good although it adds memory pressure on it. An image that's
 * used a lot can be cached to avoid time wastes.
 */
struct CachedImage
{
    /**
     * @brief Source image data.
     */
    AllocatedImage image = {};
    /**
     * @brief Descriptor set of the image on the device.
     */
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};
}

#endif // JP_GRAPHICS_ALLOCATEDIMAGE_H
