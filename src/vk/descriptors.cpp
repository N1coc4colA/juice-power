#include "descriptors.h"

#include <cassert>

#include <fmt/ostream.h>

#include "defines.h"


void DescriptorLayoutBuilder::add_binding(const uint32_t binding, const VkDescriptorType type)
{
	bindings.push_back(
	VkDescriptorSetLayoutBinding {
		.binding = binding,
		.descriptorType = type,
		.descriptorCount = 1,
	});
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, const VkShaderStageFlags shaderStages)
{
	assert(device != VK_NULL_HANDLE);

	for (auto &b : bindings) {
		b.stageFlags |= shaderStages;
	}

	const VkDescriptorSetLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = (uint32_t)bindings.size(),
		.pBindings = bindings.data(),
	};

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocator::init_pool(VkDevice device, const uint32_t maxSets, const std::span<const PoolSizeRatio> &poolRatios)
{
	assert(device != VK_NULL_HANDLE);

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(poolRatios.size());

	for (const PoolSizeRatio &ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * maxSets)
		});
	}

	const VkDescriptorPoolCreateInfo pool_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,
		.maxSets = maxSets,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	vkDestroyDescriptorPool(device,pool,nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	assert(device != VK_NULL_HANDLE);

	const VkDescriptorSetAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet ds;
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

	return ds;
}

void DescriptorWriter::write_image(const int binding, VkImageView image, VkSampler sampler,  const VkImageLayout layout, const VkDescriptorType type)
{
	assert(image != VK_NULL_HANDLE);
	assert(sampler != VK_NULL_HANDLE);

	VkDescriptorImageInfo &info = imageInfos.emplace_back(
	VkDescriptorImageInfo {
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	writes.push_back(
	VkWriteDescriptorSet {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE, //left empty for now until we need to write it
		.dstBinding = static_cast<uint32_t>(binding),
		.descriptorCount = 1,
		.descriptorType = type,
		.pImageInfo = &info,
	});
}

void DescriptorWriter::write_buffer(const int binding, VkBuffer buffer, const size_t size, const size_t offset, const VkDescriptorType type)
{
	assert(buffer != VK_NULL_HANDLE);

	VkDescriptorBufferInfo &info = bufferInfos.emplace_back(
		VkDescriptorBufferInfo {
			.buffer = buffer,
			.offset = offset,
			.range = size
	});

	writes.push_back(
		VkWriteDescriptorSet {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = VK_NULL_HANDLE, //left empty for now until we need to write it
			.dstBinding = static_cast<uint32_t>(binding),
			.descriptorCount = 1,
			.descriptorType = type,
			.pBufferInfo = &info,
	});
}

void DescriptorWriter::clear()
{
	imageInfos.clear();
	writes.clear();
	bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
	assert(device != VK_NULL_HANDLE);
	assert(set != VK_NULL_HANDLE);

	for (auto &write : writes) {
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void DescriptorAllocatorGrowable::init(VkDevice device, const uint32_t maxSets, const std::span<const PoolSizeRatio> &poolRatios)
{
	assert(device != VK_NULL_HANDLE);

	ratios.clear();

	for (const auto &r : poolRatios) {
		ratios.push_back(r);
	}

	// [REORDERED] [SWAP.0]
	setsPerPool = maxSets * 1.5; //grow it next allocation
	// [REORDERED] [SWAP.0]
	const VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);

	readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	for (const auto &p : readyPools) {
		vkResetDescriptorPool(device, p, 0);
	}
	for (const auto &p : fullPools) {
		vkResetDescriptorPool(device, p, 0);
		readyPools.push_back(p);
	}

	fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	for (const auto &p : readyPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	for (const auto &p : fullPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}

	readyPools.clear();
	fullPools.clear();
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	VkDescriptorPool newPool;

	if (readyPools.empty()) {
		//need to create a new pool
		newPool = create_pool(device, setsPerPool, ratios);

		setsPerPool = setsPerPool * 1.5;
		if (setsPerPool > 4092) {
			setsPerPool = 4092;
		}
	} else {
		newPool = readyPools.back();
		readyPools.pop_back();
	}

	return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, const uint32_t setCount, const std::span<const PoolSizeRatio> &poolRatios)
{
	assert(device != VK_NULL_HANDLE);

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(poolRatios.size());

	for (const PoolSizeRatio &ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * setCount)
		});
	}

	const VkDescriptorPoolCreateInfo pool_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,
		.maxSets = setCount,
		.poolSizeCount = (uint32_t)poolSizes.size(),
		.pPoolSizes = poolSizes.data(),
	};

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);

	return newPool;
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	assert(device != VK_NULL_HANDLE);

	//get or create a pool to allocate from
	VkDescriptorPool poolToUse = get_pool(device);

	VkDescriptorSetAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = poolToUse,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet ds;
	const VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

	//allocation failed. Try again
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY ||
		result == VK_ERROR_FRAGMENTED_POOL) {

		fullPools.push_back(poolToUse);

		poolToUse = get_pool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	readyPools.push_back(poolToUse);
	return ds;
}
