#ifndef GRAPHICS_DESCRIPTORBUILDER_H
#define GRAPHICS_DESCRIPTORBUILDER_H

#include "descriptors.h"


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


#endif // GRAPHICS_DESCRIPTORBUILDER_H
