#ifndef UTILS_H
#define UTILS_H

#include <vulkan/vulkan.h>


namespace vkutil
{

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

bool load_shader_module(const char *filePath, VkDevice device, VkShaderModule &outShaderModule);

};

#endif // UTILS_H
