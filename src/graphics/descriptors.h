#ifndef GRAPHICS_DESCRIPTORS_H
#define GRAPHICS_DESCRIPTORS_H

#include <deque>
#include <vector>
#include <span>

#include <vulkan/vulkan.h>


struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> bindings {};

	void add_binding(const uint32_t binding, const VkDescriptorType type);
	void clear();
	VkDescriptorSetLayout build(VkDevice device, const VkShaderStageFlags shaderStages);
};


struct DescriptorWriter
{
	std::deque<VkDescriptorImageInfo> imageInfos {};
	std::deque<VkDescriptorBufferInfo> bufferInfos {};
	std::vector<VkWriteDescriptorSet> writes {};

	void write_image(const int binding, VkImageView image, VkSampler sampler, const VkImageLayout layout, const VkDescriptorType type);
	void write_buffer(const int binding, VkBuffer buffer, const size_t size, const size_t offset, const VkDescriptorType type);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);
};

struct DescriptorAllocator
{

	struct PoolSizeRatio
	{
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		float ratio = 0.f;
	};

	VkDescriptorPool pool = VK_NULL_HANDLE;

	void init_pool(VkDevice device, const uint32_t maxSets, const std::span<const PoolSizeRatio> &poolRatios);
	void clear_descriptors(VkDevice device);
	void destroy_pool(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable
{
public:
	struct PoolSizeRatio
	{
		VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		float ratio = 0.f;
	};

	void init(VkDevice device, uint32_t initialSets, const std::span<const PoolSizeRatio> &poolRatios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, const uint32_t setCount, const std::span<const PoolSizeRatio> &poolRatios);

	std::vector<PoolSizeRatio> ratios {};
	std::vector<VkDescriptorPool> fullPools {};
	std::vector<VkDescriptorPool> readyPools {};
	uint32_t setsPerPool = -1;
};


#endif // GRAPHICS_DESCRIPTORS_H
