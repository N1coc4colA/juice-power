#include "src/graphics/defines.h"

#include <cstdio>

#include <fmt/ostream.h>

#include <magic_enum_flags.hpp>

namespace Graphics {

void vkCheck(const VkResult error)
{
    if (error != VK_SUCCESS) {
        fmt::print("Detected Vulkan error: {} {}\n", static_cast<int64_t>(error), magic_enum::enum_flags_name(error));
        std::fflush(nullptr);
        abort();
    }
}

} // namespace Graphics
