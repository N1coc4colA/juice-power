#include "initializers.h"

#include <cassert>


namespace Graphics
{

VkCommandPoolCreateInfo vkinit::command_pool_create_info(const VkCommandPoolCreateFlags flags)
{
	assert(flags != VK_COMMAND_POOL_CREATE_FLAG_BITS_MAX_ENUM);

	const VkCommandPoolCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
	};

	return info;
}

VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(VkCommandPool pool, const uint32_t count)
{
	assert(pool != VK_NULL_HANDLE);
	assert(count != 0);

	const VkCommandBufferAllocateInfo info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = count,
	};

	return info;
}

VkCommandBufferBeginInfo vkinit::command_buffer_begin_info(const VkCommandBufferUsageFlags flags)
{
	assert(flags != VK_COMMAND_BUFFER_USAGE_FLAG_BITS_MAX_ENUM);

	const VkCommandBufferBeginInfo info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = flags,
		.pInheritanceInfo = nullptr,
	};

	return info;
}

VkFenceCreateInfo vkinit::fence_create_info(const VkFenceCreateFlags flags)
{
	assert(flags != VK_FENCE_CREATE_FLAG_BITS_MAX_ENUM);

	const VkFenceCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
	};

	return info;
}

VkSemaphoreCreateInfo vkinit::semaphore_create_info(const VkSemaphoreCreateFlags flags)
{
	assert(flags != VK_FENCE_CREATE_FLAG_BITS_MAX_ENUM);

	const VkSemaphoreCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = flags,
	};

	return info;
}

VkSemaphoreSubmitInfo vkinit::semaphore_submit_info(const VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	assert(stageMask != VK_PIPELINE_STAGE_2_NONE);
	assert(semaphore != VK_NULL_HANDLE);

	const VkSemaphoreSubmitInfo submitInfo {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = semaphore,
		.value = 1,
		.stageMask = stageMask,
		.deviceIndex = 0,
	};

	return submitInfo;
}

VkCommandBufferSubmitInfo vkinit::command_buffer_submit_info(VkCommandBuffer cmd)
{
	assert(cmd != VK_NULL_HANDLE);

	const VkCommandBufferSubmitInfo info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext = nullptr,
		.commandBuffer = cmd,
		.deviceMask = 0,
	};

	return info;
}

VkSubmitInfo2 vkinit::submit_info(const VkCommandBufferSubmitInfo &cmd, const VkSemaphoreSubmitInfo *signalSemaphoreInfo, const VkSemaphoreSubmitInfo *waitSemaphoreInfo)
{
	const VkSubmitInfo2 info {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfo == nullptr ? 0 : 1),
		.pWaitSemaphoreInfos = waitSemaphoreInfo,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd,
		.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfo == nullptr ? 0 : 1),
		.pSignalSemaphoreInfos = signalSemaphoreInfo,
	};

	return info;
}

VkRenderingAttachmentInfo vkinit::attachment_info(VkImageView view, const VkClearValue *clear, const VkImageLayout layout)
{
	assert(view != VK_NULL_HANDLE);
	assert(layout != VK_IMAGE_LAYOUT_MAX_ENUM);

	const VkRenderingAttachmentInfo colorAttachment {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = view,
		.imageLayout = layout,
		.loadOp = clear
				? VK_ATTACHMENT_LOAD_OP_CLEAR
				: VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clear
					? *clear
					: VkClearValue{},
	};

	return colorAttachment;
}

VkRenderingAttachmentInfo vkinit::depth_attachment_info(VkImageView view, const VkImageLayout layout)
{
	assert(view != VK_NULL_HANDLE);
	assert(layout != VK_IMAGE_LAYOUT_MAX_ENUM);

	const VkRenderingAttachmentInfo depthAttachment {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = view,
		.imageLayout = layout,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {
			.depthStencil = {
				.depth = 0.f,
			},
		},
	};

	return depthAttachment;
}

VkRenderingInfo vkinit::rendering_info(const VkExtent2D &renderExtent, const VkRenderingAttachmentInfo *colorAttachment, const VkRenderingAttachmentInfo *depthAttachment)
{
	const VkRenderingInfo renderInfo {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext = nullptr,

		.renderArea = VkRect2D{VkOffset2D{0, 0}, renderExtent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = colorAttachment,
		.pDepthAttachment = depthAttachment,
		.pStencilAttachment = nullptr,
	};

	return renderInfo;
}

VkImageSubresourceRange vkinit::image_subresource_range(VkImageAspectFlags aspectMask)
{
	assert(aspectMask != VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM);

	const VkImageSubresourceRange subImage {
		.aspectMask = aspectMask,
		.baseMipLevel = 0,
		.levelCount = VK_REMAINING_MIP_LEVELS,
		.baseArrayLayer = 0,
		.layerCount = VK_REMAINING_ARRAY_LAYERS,
	};

	return subImage;
}

VkImageCreateInfo vkinit::image_create_info(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D &extent)
{
	assert(format != VK_FORMAT_MAX_ENUM);
	assert(usageFlags != VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM);

	const VkImageCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,

		.imageType = VK_IMAGE_TYPE_2D,

		.format = format,
		.extent = extent,

		.mipLevels = 1,
		.arrayLayers = 1,

		// for MSAA. we will not be using it by default, so default it to 1 sample per
		// pixel.
		.samples = VK_SAMPLE_COUNT_1_BIT,

		// optimal tiling, which means the image is stored on the best gpu format
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = usageFlags,
	};

	return info;
}

VkImageViewCreateInfo vkinit::imageview_create_info(const VkFormat format, VkImage image, const VkImageAspectFlags aspectFlags)
{
	assert(format != VK_FORMAT_MAX_ENUM);
	assert(image != VK_NULL_HANDLE);
	assert(aspectFlags != VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM);

	// build a image-view for the depth image to use for rendering
	const VkImageViewCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = aspectFlags,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	return info;
}

VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info()
{
	constexpr VkPipelineLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,

		// empty defaults
		.flags = 0,
		.setLayoutCount = 0,
		.pSetLayouts = nullptr,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};

	return info;
}

VkPipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(const VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char *entry)
{
	assert(stage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
	assert(shaderModule != VK_NULL_HANDLE);
	assert(entry != nullptr);

	const VkPipelineShaderStageCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,

		// shader stage
		.stage = stage,
		// module containing the code for this shader stage
		.module = shaderModule,
		// the entry point of the shader
		.pName = entry,
	};

	return info;
}


}
