#include "descriptors.h"

#include <algorithm>
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

auto DescriptorLayoutBuilder::build(VkDevice device, const VkShaderStageFlags shaderStages) -> VkDescriptorSetLayout
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
	vkCheck(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocator::destroyPool(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	vkDestroyDescriptorPool(device, pool, nullptr);
}

auto DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) -> VkDescriptorSet
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
	vkCheck(vkAllocateDescriptorSets(device, &allocInfo, &ds));

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
    std::ranges::copy(poolRatios, m_ratios.begin());

    m_setsPerPool = static_cast<uint32_t>(static_cast<float>(maxSets) * growthSize); //grow it next allocation
    const VkDescriptorPool newPool = createPool(device, maxSets, poolRatios);

    m_readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clearPools(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	for (const auto &p : m_readyPools) {
		vkCheck(vkResetDescriptorPool(device, p, 0));
	}
	for (const auto &p : m_fullPools) {
		vkCheck(vkResetDescriptorPool(device, p, 0));
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

auto DescriptorAllocatorGrowable::getPool(VkDevice device) -> VkDescriptorPool
{
	assert(device != VK_NULL_HANDLE);

    VkDescriptorPool newPool = VK_NULL_HANDLE;

    if (m_readyPools.empty()) {
        //need to create a new pool
        newPool = createPool(device, m_setsPerPool, m_ratios);

        m_setsPerPool = static_cast<uint32_t>(static_cast<float>(m_setsPerPool) * growthSize);
        if (m_setsPerPool > maxSetsPerPool) {
            m_setsPerPool = maxSetsPerPool;
        }
    } else {
        newPool = m_readyPools.back();
        m_readyPools.pop_back();
    }

    return newPool;
}

auto DescriptorAllocatorGrowable::createPool(VkDevice device, const uint32_t setCount, const std::span<const PoolSizeRatio> &poolRatios)
    -> VkDescriptorPool
{
	assert(device != VK_NULL_HANDLE);
	assert(setCount != 0);

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.reserve(poolRatios.size());

    std::ranges::transform(poolRatios, poolSizes.begin(), [setCount](const auto &ratio) -> auto {
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
    vkCheck(vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool));

	return newPool;
}

auto DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout) -> VkDescriptorSet
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

		vkCheck(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	m_readyPools.push_back(poolToUse);
	return ds;
}

void DescriptorAllocatorFreeable::init(VkDevice device, const uint32_t maxSets, const VkDescriptorType type)
{
    assert(device != VK_NULL_HANDLE);
    assert(maxSets != 0);

    const VkDescriptorPoolSize poolSize{
        .type = type,
        .descriptorCount = maxSets,
    };

    const VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = maxSets,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };

    vkCheck(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool));
}

void DescriptorAllocatorFreeable::destroyPool(VkDevice device)
{
    assert(device != VK_NULL_HANDLE);

    vkDestroyDescriptorPool(device, m_pool, nullptr);
    m_pool = VK_NULL_HANDLE;
}

auto DescriptorAllocatorFreeable::allocate(VkDevice device, const VkDescriptorSetLayout layout) -> VkDescriptorSet
{
    assert(device != VK_NULL_HANDLE);
    assert(layout != VK_NULL_HANDLE);
    assert(m_pool != VK_NULL_HANDLE);

    const VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet ds = VK_NULL_HANDLE;
    vkCheck(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

void DescriptorAllocatorFreeable::free(VkDevice device, const VkDescriptorSet set)
{
    assert(device != VK_NULL_HANDLE);
    assert(set != VK_NULL_HANDLE);
    assert(m_pool != VK_NULL_HANDLE);

    vkFreeDescriptorSets(device, m_pool, 1, &set);
}
}
