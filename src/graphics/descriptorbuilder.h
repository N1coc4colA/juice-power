#ifndef DESCRIPTORBUILDER_H
#define DESCRIPTORBUILDER_H

#include "src/vk/descriptors.h"


class DescriptorBuilder
{
public:
	static DescriptorBuilder begin(DescriptorAllocatorGrowable* allocator, VkDescriptorSetLayout layout);
	DescriptorBuilder& bind_image(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	bool build(VkDevice device, VkDescriptorSet& set);

private:
	std::vector<VkWriteDescriptorSet> writes {};
	DescriptorAllocatorGrowable *currentAllocator = nullptr;
	VkDescriptorSetLayout currentLayout = VK_NULL_HANDLE;
};


#endif // DESCRIPTORBUILDER_H
