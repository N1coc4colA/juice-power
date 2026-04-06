#include "src/graphics/defines.h"

#include <fmt/ostream.h>

#include <magic_enum.hpp>

namespace Graphics {

void vkCheck(const VkResult error)
{
    if (error != VK_SUCCESS) {
        fmt::print("Detected Vulkan error: {}", magic_enum::enum_name(error));
        abort();
    }
}

} // namespace Graphics
