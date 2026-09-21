#include "src/graphics/defines.h"

#include <cstdio>

#include <fmt/ostream.h>

#include <magic_enum_flags.hpp>

namespace Graphics {

// defines.cpp
void vkCheck(const VkResult error)
{
    if (error != VK_SUCCESS) {
        const auto name = magic_enum::enum_name(error);
        if (!name.empty()) {
            fmt::print("Detected Vulkan error: {} (0x{:X})\n",
                       name, static_cast<uint32_t>(error));
        } else {
            fmt::print("Detected Vulkan error: {} (0x{:X})\n",
                       static_cast<int64_t>(error), static_cast<uint32_t>(error));
        }
        std::fflush(nullptr);
        abort();
    }
}

} // namespace Graphics
