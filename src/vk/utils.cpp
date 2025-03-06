#include "utils.h"

#include <cassert>
#include <fstream>
#include <vector>

#include "initializers.h"


namespace vkutil
{

void transition_image(VkCommandBuffer cmd, VkImage image, const VkImageLayout currentLayout, const VkImageLayout newLayout)
{
	assert(cmd != VK_NULL_HANDLE);
	assert(image != VK_NULL_HANDLE);

	const VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
			? VK_IMAGE_ASPECT_DEPTH_BIT
			: VK_IMAGE_ASPECT_COLOR_BIT;

	const VkImageMemoryBarrier2 imageBarrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
		.oldLayout = currentLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = 0,
		.dstQueueFamilyIndex = 0,
		.image = image,
		.subresourceRange = vkinit::image_subresource_range(aspectMask),
	};

	const VkDependencyInfo depInfo{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &imageBarrier,
	};

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, const VkExtent2D &srcSize, const VkExtent2D &dstSize)
{
	assert(cmd != VK_NULL_HANDLE);
	assert(source != VK_NULL_HANDLE);
	assert(destination != VK_NULL_HANDLE);

	const VkImageBlit2 blitRegion{
		.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
		.pNext = nullptr,
		.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.srcOffsets = {
			{},
			{
				.x = static_cast<int32_t>(srcSize.width),
				.y = static_cast<int32_t>(srcSize.height),
				.z = 1,
			},
		},
		.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.dstOffsets = {
			{},
			{
				.x = static_cast<int32_t>(dstSize.width),
				.y = static_cast<int32_t>(dstSize.height),
				.z = 1,
			},
		},
	};

	const VkBlitImageInfo2 blitInfo{
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.pNext = nullptr,
		.srcImage = source,
		.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.dstImage = destination,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = 1,
		.pRegions = &blitRegion,
		.filter = VK_FILTER_LINEAR,
	};

	vkCmdBlitImage2(cmd, &blitInfo);
}

bool load_shader_module(const char *filePath, VkDevice device, VkShaderModule &outShaderModule)
{
	assert(filePath != nullptr);
	assert(device != VK_NULL_HANDLE);

	// open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return false;
	}

	// find what the size of the file is by looking up the location of the cursor
	// because the cursor is at the end, it gives the size directly in bytes
	const size_t fileSize = static_cast<size_t>(file.tellg());

	// spirv expects the buffer to be on uint32, so make sure to reserve a int
	// vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	// put file cursor at beginning
	file.seekg(0);

	// load the entire file into the buffer
	file.read(reinterpret_cast<char *>(buffer.data()), fileSize);

	// now that the file is loaded into the buffer, we can close it
	file.close();

	// create a new shader module, using the buffer we loaded
	const VkShaderModuleCreateInfo createInfo {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,

		// codeSize has to be in bytes, so multply the ints in the buffer by size of
		// int to know the real size of the buffer
		.codeSize = buffer.size() * sizeof(uint32_t),
		.pCode = buffer.data(),
	};

	// check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}

	outShaderModule = shaderModule;

	return true;
}


}
