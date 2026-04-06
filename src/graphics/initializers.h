#ifndef GRAPHICS_INITIALIZERS_H
#define GRAPHICS_INITIALIZERS_H

#include <vulkan/vulkan.h>

namespace Graphics::Init {

auto commandPoolCreateInfo(VkCommandPoolCreateFlags flags = 0) -> VkCommandPoolCreateInfo;
auto commandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1) -> VkCommandBufferAllocateInfo;

auto commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0) -> VkCommandBufferBeginInfo;
auto commandBufferSubmitInfo(VkCommandBuffer cmd) -> VkCommandBufferSubmitInfo;

auto fenceCreateInfo(VkFenceCreateFlags flags = 0) -> VkFenceCreateInfo;

auto semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0) -> VkSemaphoreCreateInfo;

auto submitInfo(const VkCommandBufferSubmitInfo &cmd, const VkSemaphoreSubmitInfo *signalSemaphoreInfo, const VkSemaphoreSubmitInfo *waitSemaphoreInfo)
    -> VkSubmitInfo2;

auto attachmentInfo(VkImageView view, const VkClearValue *clear, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    -> VkRenderingAttachmentInfo;

auto depthAttachmentInfo(VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) -> VkRenderingAttachmentInfo;

auto renderingInfo(const VkExtent2D &renderExtent, const VkRenderingAttachmentInfo *colorAttachment, const VkRenderingAttachmentInfo *depthAttachment)
    -> VkRenderingInfo;

auto imageSubresourceRange(VkImageAspectFlags aspectMask) -> VkImageSubresourceRange;

auto semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) -> VkSemaphoreSubmitInfo;

auto imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, const VkExtent3D &extent) -> VkImageCreateInfo;
auto imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags) -> VkImageViewCreateInfo;
auto pipelineLayoutCreateInfo() -> VkPipelineLayoutCreateInfo;
auto pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule, const char *entry = "main")
    -> VkPipelineShaderStageCreateInfo;
} // namespace Graphics::Init

#endif // GRAPHICS_INITIALIZERS_H
