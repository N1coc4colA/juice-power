#include "descriptors.h"

#include <cassert>

#include <fmt/ostream.h>

#include "defines.h"


namespace Graphics
{

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

    const VkDescriptorSetLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout set = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
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

	const VkDescriptorBufferInfo &info = bufferInfos.emplace_back(VkDescriptorBufferInfo {
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

	m_ratios.resize(poolRatios.size());
	std::copy(poolRatios.cbegin(), poolRatios.cend(), m_ratios.begin());

	m_setsPerPool = static_cast<uint32_t>(static_cast<float>(maxSets) * 1.5f); //grow it next allocation
	VkDescriptorPool newPool = createPool(device, maxSets, poolRatios);

	m_readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clearPools(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	for (const auto &p : m_readyPools) {
		VK_CHECK(vkResetDescriptorPool(device, p, 0));
	}
	for (const auto &p : m_fullPools) {
		VK_CHECK(vkResetDescriptorPool(device, p, 0));
		m_readyPools.push_back(p);
	}

	m_fullPools.clear();
}

void DescriptorAllocatorGrowable::destroyPools(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	for (const auto &p : m_readyPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	for (const auto &p : m_fullPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}

	m_readyPools.clear();
	m_fullPools.clear();
}

VkDescriptorPool DescriptorAllocatorGrowable::getPool(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	VkDescriptorPool newPool;

	if (m_readyPools.empty()) {
		//need to create a new pool
		newPool = createPool(device, m_setsPerPool, m_ratios);

		m_setsPerPool = static_cast<uint32_t>(static_cast<float>(m_setsPerPool) * 1.5f);
		if (m_setsPerPool > 4092) {
			m_setsPerPool = 4092;
		}
	} else {
		newPool = m_readyPools.back();
		m_readyPools.pop_back();
	}

	return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::createPool(VkDevice device, const uint32_t setCount, const std::span<const PoolSizeRatio> &poolRatios)
{
	assert(device != VK_NULL_HANDLE);
	assert(setCount != 0);

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(poolRatios.size());

    std::transform(poolRatios.cbegin(), poolRatios.cend(), poolSizes.begin(), [setCount](const auto &ratio) {
        return VkDescriptorPoolSize{.type = ratio.type, .descriptorCount = static_cast<uint32_t>(ratio.ratio * static_cast<float>(setCount))};
    });

    const VkDescriptorPoolCreateInfo pool_info{
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

		m_fullPools.push_back(poolToUse);

		poolToUse = getPool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	m_readyPools.push_back(poolToUse);
	return ds;
}


}
