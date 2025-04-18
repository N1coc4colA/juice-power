#include "initializers.h"

#include <cassert>


namespace Graphics
{

VkCommandPoolCreateInfo vkinit::command_pool_create_info(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags)
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

VkPresentInfoKHR vkinit::present_info()
{
	constexpr VkPresentInfoKHR info {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = 0,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.swapchainCount = 0,
		.pSwapchains = nullptr,
		.pImageIndices = nullptr,
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

VkDescriptorSetLayoutBinding vkinit::descriptorset_layout_binding(const VkDescriptorType type, const VkShaderStageFlags stageFlags, const uint32_t binding)
{
	assert(type != VK_DESCRIPTOR_TYPE_MAX_ENUM);
	assert(stageFlags != VK_PIPELINE_LAYOUT_CREATE_FLAG_BITS_MAX_ENUM);

	const VkDescriptorSetLayoutBinding setbind {
		.binding = binding,
		.descriptorType = type,
		.descriptorCount = 1,
		.stageFlags = stageFlags,
		.pImmutableSamplers = nullptr,
	};

	return setbind;
}

VkDescriptorSetLayoutCreateInfo vkinit::descriptorset_layout_create_info(const VkDescriptorSetLayoutBinding &bindings, const uint32_t bindingCount)
{
	assert(bindingCount != 0);

	const VkDescriptorSetLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = bindingCount,
		.pBindings = &bindings,
	};

	return info;
}

VkWriteDescriptorSet vkinit::write_descriptor_image(const VkDescriptorType type, VkDescriptorSet dstSet, const VkDescriptorImageInfo &imageInfo, const uint32_t binding)
{
	assert(type != VK_DESCRIPTOR_TYPE_MAX_ENUM);
	assert(dstSet != VK_NULL_HANDLE);

	const VkWriteDescriptorSet write {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = dstSet,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pImageInfo = &imageInfo,
	};

	return write;
}

VkWriteDescriptorSet vkinit::write_descriptor_buffer(const VkDescriptorType type, VkDescriptorSet dstSet, const VkDescriptorBufferInfo &bufferInfo, const uint32_t binding)
{
	assert(type != VK_DESCRIPTOR_TYPE_MAX_ENUM);
	assert(dstSet != VK_NULL_HANDLE);

	const VkWriteDescriptorSet write {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = dstSet,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pBufferInfo = &bufferInfo,
	};

	return write;
}

VkDescriptorBufferInfo vkinit::buffer_info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	assert(buffer != VK_NULL_HANDLE);
	assert(range != 0);

	const VkDescriptorBufferInfo binfo {
		.buffer = buffer,
		.offset = offset,
		.range = range,
	};

	return binfo;
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

VkSamplerCreateInfo vkinit::sampler_create_info(const VkFilter filters, const VkSamplerAddressMode samplerAddressMode)
{
	const VkSamplerCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = filters,
		.minFilter = filters,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = samplerAddressMode,
		.addressModeV = samplerAddressMode,
		.addressModeW = samplerAddressMode,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		//.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 16.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = VK_LOD_CLAMP_NONE,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	return info;
}

VkPipelineVertexInputStateCreateInfo vkinit::vertex_input_state_create_info()
{
	constexpr VkPipelineVertexInputStateCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};

	return info;
}

VkPipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(const VkPrimitiveTopology topology)
{
	const VkPipelineInputAssemblyStateCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = topology,
		.primitiveRestartEnable = VK_FALSE,
	};

	return info;
}

VkPipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(const VkPolygonMode polygonMode)
{
	VkPipelineRasterizationStateCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = polygonMode,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	return info;
}

VkPipelineMultisampleStateCreateInfo vkinit::multisampling_state_create_info()
{
	constexpr VkPipelineMultisampleStateCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
	};

	return info;
}

VkPipelineColorBlendAttachmentState vkinit::color_blend_attachment_state()
{
	constexpr VkPipelineColorBlendAttachmentState info {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
						  VK_COLOR_COMPONENT_G_BIT |
						  VK_COLOR_COMPONENT_B_BIT |
						  VK_COLOR_COMPONENT_A_BIT,
	};

	return info;
}

VkPipelineDepthStencilStateCreateInfo vkinit::depth_stencil_create_info(const bool bDepthTest, const bool bDepthWrite, const VkCompareOp compareOp)
{
	const VkPipelineDepthStencilStateCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE,
		.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE,
		.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
	};

	return info;
}


}
