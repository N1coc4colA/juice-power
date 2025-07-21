#ifndef GRAPHICS_DESCRIPTORS_H
#define GRAPHICS_DESCRIPTORS_H

#include <deque>
#include <vector>
#include <span>

#include <vulkan/vulkan.h>


namespace Graphics
{

/**
 * @brief Builder for VkDescriptorSetLayout objects
 *
 * Simplifies the process of creating descriptor set layouts by managing bindings
 * and providing a clean interface for construction.
 */
struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> bindings {};

	void addBinding(const uint32_t binding, const VkDescriptorType type);

	/**
	 * @brief Clears all bindings from the builder
	 */
	void clear();

	/**
	 * @brief Constructs a VkDescriptorSetLayout from the current bindings
	 * @param device The Vulkan device to create the layout on
	 * @param shaderStages Shader stages that will use these descriptors
	 * @return The created descriptor set layout
	 */
	VkDescriptorSetLayout build(VkDevice device, const VkShaderStageFlags shaderStages);
};


/**
 * @brief Helper for writing updates to descriptor sets
 *
 * Manages descriptor writes and their associated memory (image/buffer infos)
 * allowing batched updates to descriptor sets.
 */
struct DescriptorWriter
{
	std::deque<VkDescriptorImageInfo> imageInfos {};
	std::deque<VkDescriptorBufferInfo> bufferInfos {};
	std::vector<VkWriteDescriptorSet> writes {};

	/**
	 * @brief Queues an image descriptor write
	 * @param binding The binding number to update
	 * @param image The image view to use
	 * @param sampler The sampler to use (if needed)
	 * @param layout The current layout of the image
	 * @param type The descriptor type being written
	 */
	void writeImage(const int binding, VkImageView image, VkSampler sampler, const VkImageLayout layout, const VkDescriptorType type);

	/**
	 * @brief Queues a buffer descriptor write
	 * @param binding The binding number to update
	 * @param buffer The buffer to use
	 * @param size The size of the buffer range
	 * @param offset The offset into the buffer
	 * @param type The descriptor type being written
	 */
	void writeBuffer(const int binding, VkBuffer buffer, const size_t size, const size_t offset, const VkDescriptorType type);

	/**
	 * @brief Clears all pending writes and associated info
	 */
	void clear();

	/**
	 * @brief Applies all queued writes to a descriptor set
	 * @param device The Vulkan device
	 * @param set The descriptor set to update
	 */
	void updateSet(VkDevice device, VkDescriptorSet set);
};

/**
 * @brief Fixed-size descriptor allocator
 *
 * Manages a single descriptor pool with fixed ratios of descriptor types.
 * More efficient than the growable version when descriptor requirements are known.
 */
struct DescriptorAllocator
{
	/**
	 * @brief Descriptor type to allocation ratio
	 */
	struct PoolSizeRatio
	{
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		float ratio = 0.f;
	};

	VkDescriptorPool pool = VK_NULL_HANDLE;

	/**
	 * @brief Destroys the descriptor pool
	 * @param device The Vulkan device
	 */
	void destroyPool(VkDevice device);

	/**
	 * @brief Allocates a descriptor set
	 * @param device The Vulkan device
	 * @param layout The layout for the descriptor set
	 * @return The allocated descriptor set
	 */
	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

/**
 * @brief Growable descriptor allocator
 *
 * Automatically creates new pools as needed when allocations fail.
 * More flexible than the fixed version when descriptor requirements are dynamic.
 */
struct DescriptorAllocatorGrowable
{
public:
	/**
	 * @brief Descriptor type for allocation ratio
	 */
	struct PoolSizeRatio
	{
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		float ratio = 0.f;
	};

	/**
	 * @brief Initializes the allocator
	 * @param device The Vulkan device
	 * @param initialSets Number of sets for the first pool
	 * @param poolRatios Ratios of different descriptor types in pools
	 */
	void init(VkDevice device, uint32_t initialSets, const std::span<const PoolSizeRatio> &poolRatios);

	/**
	 * @brief Resets all pools
	 * @param device The Vulkan device
	 */
	void clearPools(VkDevice device);

	/**
	 * @brief Destroys all pools
	 * @param device The Vulkan device
	 */
	void destroyPools(VkDevice device);

	/**
	 * @brief Allocates a descriptor set
	 * @param device The Vulkan device
	 * @param layout The layout for the descriptor set
	 * @return The allocated descriptor set
	 */
	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:
	/**
	 * @brief Gets an available pool or creates a new one
	 * @param device The Vulkan device
	 * @return A pool ready for allocations
	 */
	VkDescriptorPool getPool(VkDevice device);

	/**
	 * @brief Creates a new descriptor pool
	 * @param device The Vulkan device
	 * @param setCount Number of sets the pool should hold
	 * @param poolRatios Ratios of descriptor types
	 * @return The created descriptor pool
	 */
	VkDescriptorPool createPool(VkDevice device, const uint32_t setCount, const std::span<const PoolSizeRatio> &poolRatios);

    std::vector<PoolSizeRatio> m_ratios{};
    std::vector<VkDescriptorPool> m_fullPools{};
    std::vector<VkDescriptorPool> m_readyPools{};
    uint32_t m_setsPerPool = -1;
};


}


#endif // GRAPHICS_DESCRIPTORS_H
