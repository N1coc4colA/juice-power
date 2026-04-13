#ifndef JP_GRAPHICS_DEFINES_H
#define JP_GRAPHICS_DEFINES_H

#include <vulkan/vulkan_core.h>

namespace Graphics {
/**
 * @brief Checks for success and generates, may generate a message and exit the process.
 * Sometimes API failures can not be handled gracefully, or just means that the process
 * can not continue normally anyway. For these reasons, just printing and aborting is
 * better, which is what this function does.
 * @param err Result of a previous call to the Vulkan API.
 */
void vkCheck(VkResult error);
} // namespace Graphics

#endif // JP_GRAPHICS_DEFINES_H
