#ifndef GRAPHICS_UTILS_H
#define GRAPHICS_UTILS_H

#include <vulkan/vulkan.h>


namespace Graphics::vkutil
{

void transition_image(VkCommandBuffer cmd, VkImage image, const VkImageLayout currentLayout, const VkImageLayout newLayout);

void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, const VkExtent2D &srcSize, const VkExtent2D &dstSize);

bool load_shader_module(const char *filePath, VkDevice device, VkShaderModule &outShaderModule);

void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);


}


#endif // GRAPHICS_UTILS_H
