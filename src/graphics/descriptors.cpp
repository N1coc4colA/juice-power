#include "descriptors.h"

#include <cassert>

#include <fmt/ostream.h>

#include "defines.h"


void DescriptorLayoutBuilder::addBinding(const uint32_t binding, const VkDescriptorType type)
{
	assert(type != VK_DESCRIPTOR_TYPE_MAX_ENUM);

	bindings.push_back(VkDescriptorSetLayoutBinding {
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
	assert(shaderStages != VK_PIPELINE_LAYOUT_CREATE_FLAG_BITS_MAX_ENUM);

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

	VkDescriptorSetLayout set = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocator::initPool(VkDevice device, const uint32_t maxSets, const std::span<const PoolSizeRatio> &poolRatios)
{
	assert(device != VK_NULL_HANDLE);
	assert(maxSets != 0);

	std::vector<VkDescriptorPoolSize> poolSizes = {};
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

	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
}

void DescriptorAllocator::clearDescriptors(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	VK_CHECK(vkResetDescriptorPool(device, pool, 0));
}

void DescriptorAllocator::destroyPool(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	assert(device != VK_NULL_HANDLE);
	assert(layout != VK_NULL_HANDLE);

	const VkDescriptorSetAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet ds = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

	return ds;
}

void DescriptorWriter::writeImage(const int binding, VkImageView image, VkSampler sampler,  const VkImageLayout layout, const VkDescriptorType type)
{
	assert(image != VK_NULL_HANDLE);
	assert(layout != VK_IMAGE_LAYOUT_MAX_ENUM);
	assert(type != VK_DESCRIPTOR_TYPE_MAX_ENUM);

	VkDescriptorImageInfo &info = imageInfos.emplace_back(VkDescriptorImageInfo {
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	writes.push_back(VkWriteDescriptorSet {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE, //left empty for now until we need to write it
		.dstBinding = static_cast<uint32_t>(binding),
		.descriptorCount = 1,
		.descriptorType = type,
		.pImageInfo = &info,
	});
}

void DescriptorWriter::writeBuffer(const int binding, VkBuffer buffer, const size_t size, const size_t offset, const VkDescriptorType type)
{
	assert(buffer != VK_NULL_HANDLE);
	assert(size != 0);
	assert(type != VK_DESCRIPTOR_TYPE_MAX_ENUM);

	VkDescriptorBufferInfo &info = bufferInfos.emplace_back(VkDescriptorBufferInfo {
		.buffer = buffer,
		.offset = offset,
		.range = size
	});

	writes.push_back(VkWriteDescriptorSet {
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

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set)
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
	assert(maxSets != 0);

	ratios.clear();
	for (const auto &r : poolRatios) {
		ratios.push_back(r);
	}

	setsPerPool = maxSets * 1.5; //grow it next allocation
	const VkDescriptorPool newPool = createPool(device, maxSets, poolRatios);

	readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clearPools(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	for (const auto &p : readyPools) {
		VK_CHECK(vkResetDescriptorPool(device, p, 0));
	}
	for (const auto &p : fullPools) {
		VK_CHECK(vkResetDescriptorPool(device, p, 0));
		readyPools.push_back(p);
	}

	fullPools.clear();
}

void DescriptorAllocatorGrowable::destroyPools(VkDevice device)
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

VkDescriptorPool DescriptorAllocatorGrowable::getPool(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	VkDescriptorPool newPool;

	if (readyPools.empty()) {
		//need to create a new pool
		newPool = createPool(device, setsPerPool, ratios);

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

VkDescriptorPool DescriptorAllocatorGrowable::createPool(VkDevice device, const uint32_t setCount, const std::span<const PoolSizeRatio> &poolRatios)
{
	assert(device != VK_NULL_HANDLE);
	assert(setCount != 0);

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(poolRatios.size());

	for (const PoolSizeRatio &ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount),
		});
	}

	const VkDescriptorPoolCreateInfo pool_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,
		.maxSets = setCount,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	VkDescriptorPool newPool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool));

	return newPool;
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	assert(device != VK_NULL_HANDLE);
	assert(layout != VK_NULL_HANDLE);

	//get or create a pool to allocate from
	VkDescriptorPool poolToUse = getPool(device);

	VkDescriptorSetAllocateInfo allocInfo {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = poolToUse,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet ds = VK_NULL_HANDLE;
	const VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

	//allocation failed. Try again
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY ||
		result == VK_ERROR_FRAGMENTED_POOL) {

		fullPools.push_back(poolToUse);

		poolToUse = getPool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	readyPools.push_back(poolToUse);
	return ds;
}
