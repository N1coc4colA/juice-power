#ifndef GRAPHICS_DESCRIPTORBUILDER_H
#define GRAPHICS_DESCRIPTORBUILDER_H

#include "descriptors.h"


namespace Graphics
{

/**
 * @brief Builder class for constructing Vulkan descriptor sets with a fluent interface
 *
 * Provides a convenient way to create and configure descriptor sets by chaining binding operations.
 * Manages the underlying descriptor writes and allocation automatically.
 */
class DescriptorBuilder
{
public:
	/**
	 * @brief Begins construction of a new descriptor set
	 * @param allocator The growable descriptor allocator to use for set allocation
	 * @param layout The descriptor set layout defining the set's structure
	 * @return A DescriptorBuilder instance ready for configuration
	 *
	 * @note This static factory method starts the builder pattern chain
	 */
	static DescriptorBuilder begin(DescriptorAllocatorGrowable *allocator, VkDescriptorSetLayout layout);

	/**
	 * @brief Binds an image to a descriptor set binding point
	 * @param binding The binding index to update (must match layout)
	 * @param imageView The image view to bind
	 * @param sampler The sampler to use (for combined image samplers)
	 * @param layout The current layout of the image
	 * @param type The descriptor type (e.g. VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	 * @return Reference to this builder for method chaining
	 *
	 * @warning The binding parameters must match the set layout's binding definitions
	 */
	DescriptorBuilder &bindImage(const uint32_t binding, VkImageView imageView, VkSampler sampler, const VkImageLayout layout, const VkDescriptorType type);

	/**
	 * @brief Finalizes and allocates the descriptor set
	 * @param device The Vulkan logical device
	 * @param[out] set Reference to receive the created descriptor set
	 * @return true if creation succeeded, false on failure
	 *
	 * @post On success, 'set' contains a valid descriptor set ready for use
	 * @post On failure, 'set' is set to VK_NULL_HANDLE
	 */
	bool build(VkDevice device, VkDescriptorSet &set);

private:
	std::vector<VkWriteDescriptorSet> writes {};
	DescriptorAllocatorGrowable *currentAllocator = nullptr;
	VkDescriptorSetLayout currentLayout = VK_NULL_HANDLE;
};


}


#endif // GRAPHICS_DESCRIPTORBUILDER_H
