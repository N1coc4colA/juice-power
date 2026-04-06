#ifndef GRAPHICS_DESCRIPTORS_H
#define GRAPHICS_DESCRIPTORS_H

#include <vulkan/vulkan.h>

#include <deque>
#include <span>
#include <vector>

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
    /**
	 * @brief Registered layout bindings of the builder.
	 */
    std::vector<VkDescriptorSetLayoutBinding> bindings{};

    /**
     * @brief Adds one binding entry to the descriptor layout builder.
     * Builds a VkDescriptorSetLayoutBinding from the parameters.
     */
    void addBinding(uint32_t binding, VkDescriptorType type);

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
    auto build(VkDevice device, VkShaderStageFlags shaderStages) -> VkDescriptorSetLayout;
};


/**
 * @brief Helper for writing updates to descriptor sets
 *
 * Manages descriptor writes and their associated memory (image/buffer infos)
 * allowing batched updates to descriptor sets.
 */
struct DescriptorWriter
{
	/// @brief Queued image descriptor payloads.
	std::deque<VkDescriptorImageInfo> imageInfos {};
	/// @brief Queued buffer descriptor payloads.
	std::deque<VkDescriptorBufferInfo> bufferInfos {};
	/// @brief Pending write operations.
	std::vector<VkWriteDescriptorSet> writes {};

	/**
	 * @brief Queues an image descriptor write
	 * @param binding The binding number to update
	 * @param image The image view to use
	 * @param sampler The sampler to use (if needed)
	 * @param layout The current layout of the image
	 * @param type The descriptor type being written
	 */
	void writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);

	/**
	 * @brief Queues a buffer descriptor write
	 * @param binding The binding number to update
	 * @param buffer The buffer to use
	 * @param size The size of the buffer range
	 * @param offset The offset into the buffer
	 * @param type The descriptor type being written
	 */
	void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

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
		/// @brief Descriptor type.
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		/// @brief Ratio of descriptors per set.
		float ratio = 0.f;
	};

	/// @brief Backing Vulkan descriptor pool.
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
    auto allocate(VkDevice device, VkDescriptorSetLayout layout) -> VkDescriptorSet;
};

/**
 * @brief Growable descriptor allocator
 *
 * Automatically creates new pools as needed when allocations fail.
 * More flexible than the fixed version when descriptor requirements are dynamic.
 */
struct DescriptorAllocatorGrowable
{
    /// @brief Pool size growth multiplier.
    static constexpr float growthSize = 1.5f;
    /// @brief Upper bound of sets per pool.
    static constexpr uint32_t maxSetsPerPool = 4092;

    /**
	 * @brief Descriptor type for allocation ratio
	 */
	struct PoolSizeRatio
	{
		/// @brief Descriptor type.
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		/// @brief Ratio of descriptors per set.
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
    auto allocate(VkDevice device, VkDescriptorSetLayout layout) -> VkDescriptorSet;

private:
	/**
	 * @brief Gets an available pool or creates a new one
	 * @param device The Vulkan device
	 * @return A pool ready for allocations
	 */
    auto getPool(VkDevice device) -> VkDescriptorPool;

    /**
	 * @brief Creates a new descriptor pool
	 * @param device The Vulkan device
	 * @param setCount Number of sets the pool should hold
	 * @param poolRatios Ratios of descriptor types
	 * @return The created descriptor pool
	 */
    static auto createPool(VkDevice device, uint32_t setCount, const std::span<const PoolSizeRatio> &poolRatios) -> VkDescriptorPool;

    /// @brief Ratios used to create new pools.
    std::vector<PoolSizeRatio> m_ratios{};
    /// @brief Pools that are currently exhausted.
    std::vector<VkDescriptorPool> m_fullPools{};
    /// @brief Pools ready for new allocations.
    std::vector<VkDescriptorPool> m_readyPools{};
    /// @brief Current set count target per pool.
    uint32_t m_setsPerPool = -1;
};

/**
 * @brief Fixed-size descriptor allocator with per-set free support.
 *
 * Backed by a pool created with FREE_DESCRIPTOR_SET_BIT.
 * Intended for long-lived descriptors that may be individually released.
 */
struct DescriptorAllocatorFreeable
{
    /// @brief Creates the freeable descriptor pool.
    void init(VkDevice device, uint32_t maxSets, VkDescriptorType type);
    /// @brief Destroys the freeable descriptor pool.
    void destroyPool(VkDevice device);

    /// @brief Allocates one descriptor set.
    auto allocate(VkDevice device, VkDescriptorSetLayout layout) -> VkDescriptorSet;
    /// @brief Frees one descriptor set.
    void free(VkDevice device, VkDescriptorSet set);

private:
    /// @brief Backing Vulkan descriptor pool.
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
}


#endif // GRAPHICS_DESCRIPTORS_H
