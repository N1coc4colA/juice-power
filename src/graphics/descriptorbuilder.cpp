#include "descriptorbuilder.h"

#include <cassert>


DescriptorBuilder DescriptorBuilder::begin(DescriptorAllocatorGrowable* allocator, VkDescriptorSetLayout layout) {
	DescriptorBuilder builder;
	builder.currentAllocator = allocator;
	builder.currentLayout = layout;
	return builder;
}

DescriptorBuilder& DescriptorBuilder::bind_image(uint32_t binding, VkImageView imageView, VkSampler sampler,
                              VkImageLayout layout, VkDescriptorType type) {
	VkDescriptorImageInfo imageInfo = {
	    .sampler = sampler,
	    .imageView = imageView,
	    .imageLayout = layout
	};

	VkWriteDescriptorSet write = {
	    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	    .dstBinding = binding,
	    .dstArrayElement = 0,
	    .descriptorCount = 1,
	    .descriptorType = type,
	    .pImageInfo = &imageInfo
	};

	writes.push_back(write);
	return *this;
}

/*bool DescriptorBuilder::build(VkDevice device, VkDescriptorSet &set)
{
	assert(device != VK_NULL_HANDLE);
	assert(currentAllocator != nullptr);
	assert(currentLayout != VK_NULL_HANDLE);

	bool success = currentAllocator->allocate(device, currentLayout);
	if (!success) {
		return false;
	}

	for (auto &write : writes) {
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	return true;
}*/

bool DescriptorBuilder::build(VkDevice device, VkDescriptorSet& set) {
	assert(device != VK_NULL_HANDLE);
	assert(currentAllocator != nullptr);
	assert(currentLayout != VK_NULL_HANDLE);

	// Actually get the descriptor set from the allocator
	set = currentAllocator->allocate(device, currentLayout);
	if (set == VK_NULL_HANDLE) {
		return false;
	}

	// Update all writes to point to our new descriptor set
	for (auto& write : writes) {
		write.dstSet = set;

		// Additional validation for safety
		assert(write.descriptorType != VK_DESCRIPTOR_TYPE_MAX_ENUM);
		if (write.pImageInfo) {
			assert(write.pImageInfo->imageView != VK_NULL_HANDLE);
			assert(write.pImageInfo->sampler != VK_NULL_HANDLE);
		}
		if (write.pBufferInfo) {
			assert(write.pBufferInfo->buffer != VK_NULL_HANDLE);
		}
	}

	// Only update if we have writes to perform
	if (!writes.empty()) {
		vkUpdateDescriptorSets(device,
		                       static_cast<uint32_t>(writes.size()),
		                       writes.data(),
		                       0, nullptr);
	}

	return true;
}
