#ifndef GRAPHICS_INITIALIZERS_H
#define GRAPHICS_INITIALIZERS_H

#include <vulkan/vulkan.h>


namespace Graphics::vkinit
{

VkCommandPoolCreateInfo command_pool_create_info(const VkCommandPoolCreateFlags flags = 0);
VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, const uint32_t count = 1);

VkCommandBufferBeginInfo command_buffer_begin_info(const VkCommandBufferUsageFlags flags = 0);
VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);

VkFenceCreateInfo fence_create_info(const VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphore_create_info(const VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo2 submit_info(const VkCommandBufferSubmitInfo &cmd, const VkSemaphoreSubmitInfo *signalSemaphoreInfo, const VkSemaphoreSubmitInfo *waitSemaphoreInfo);

VkRenderingAttachmentInfo attachment_info(VkImageView view, const VkClearValue *clear, const VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, const VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

VkRenderingInfo rendering_info(const VkExtent2D &renderExtent, const VkRenderingAttachmentInfo *colorAttachment, const VkRenderingAttachmentInfo *depthAttachment);

VkImageSubresourceRange image_subresource_range(const VkImageAspectFlags aspectMask);

VkSemaphoreSubmitInfo semaphore_submit_info(const VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);

VkImageCreateInfo image_create_info(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D &extent);
VkImageViewCreateInfo imageview_create_info(const VkFormat format, VkImage image, const VkImageAspectFlags aspectFlags);
VkPipelineLayoutCreateInfo pipeline_layout_create_info();
VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(const VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char *entry = "main");


}


#endif // GRAPHICS_INITIALIZERS_H
