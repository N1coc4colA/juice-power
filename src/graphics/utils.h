#ifndef GRAPHICS_UTILS_H
#define GRAPHICS_UTILS_H

#include <vulkan/vulkan.h>

namespace Graphics::Utils {

void transitionImage(VkCommandBuffer cmd, VkImage image, const VkImageLayout currentLayout, const VkImageLayout newLayout);

void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, const VkExtent2D &srcSize, const VkExtent2D &dstSize);

auto loadShaderModule(const char *filePath, VkDevice device, VkShaderModule &outShaderModule) -> bool;

void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
} // namespace Graphics::Utils

#endif // GRAPHICS_UTILS_H
