#include "defines.h"

#include <fmt/ostream.h>
#include <magic_enum.hpp>

namespace Graphics {

void vkCheck(const VkResult err)
{
    if (err != VK_SUCCESS) {
        fmt::print("Detected Vulkan error: {}", magic_enum::enum_name(err));
        abort();
    }
}

} // namespace Graphics
