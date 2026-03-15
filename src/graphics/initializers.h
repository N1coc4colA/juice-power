#ifndef GRAPHICS_INITIALIZERS_H
#define GRAPHICS_INITIALIZERS_H

#include <vulkan/vulkan.h>


namespace Graphics::vkinit
{

auto commandPoolCreateInfo(const VkCommandPoolCreateFlags flags = 0) -> VkCommandPoolCreateInfo;
auto commandBufferAllocateInfo(VkCommandPool pool, const uint32_t count = 1) -> VkCommandBufferAllocateInfo;

auto commandBufferBeginInfo(const VkCommandBufferUsageFlags flags = 0) -> VkCommandBufferBeginInfo;
auto commandBufferSubmitInfo(VkCommandBuffer cmd) -> VkCommandBufferSubmitInfo;

auto fenceCreateInfo(const VkFenceCreateFlags flags = 0) -> VkFenceCreateInfo;

auto semaphoreCreateInfo(const VkSemaphoreCreateFlags flags = 0) -> VkSemaphoreCreateInfo;

auto submitInfo(const VkCommandBufferSubmitInfo &cmd, const VkSemaphoreSubmitInfo *signalSemaphoreInfo, const VkSemaphoreSubmitInfo *waitSemaphoreInfo)
    -> VkSubmitInfo2;

auto attachmentInfo(VkImageView view, const VkClearValue *clear, const VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    -> VkRenderingAttachmentInfo;

auto depthAttachmentInfo(VkImageView view, const VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) -> VkRenderingAttachmentInfo;

auto renderingInfo(const VkExtent2D &renderExtent, const VkRenderingAttachmentInfo *colorAttachment, const VkRenderingAttachmentInfo *depthAttachment)
    -> VkRenderingInfo;

auto imageSubresourceRange(const VkImageAspectFlags aspectMask) -> VkImageSubresourceRange;

auto semaphoreSubmitInfo(const VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) -> VkSemaphoreSubmitInfo;

auto imageCreateInfo(const VkFormat format, const VkImageUsageFlags usageFlags, const VkExtent3D &extent) -> VkImageCreateInfo;
auto imageViewCreateInfo(const VkFormat format, VkImage image, const VkImageAspectFlags aspectFlags) -> VkImageViewCreateInfo;
auto pipelineLayoutCreateInfo() -> VkPipelineLayoutCreateInfo;
auto pipelineShaderStageCreateInfo(const VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char *entry = "main")
    -> VkPipelineShaderStageCreateInfo;
}


#endif // GRAPHICS_INITIALIZERS_H
