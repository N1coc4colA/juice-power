#ifndef DEFINES_H
#define DEFINES_H

#include <magic_enum.hpp>

#include "../defines.h"

#define VK_CHECK(x) \
    do { \
        const VkResult err = x; \
        if (err != VK_SUCCESS) { \
            fmt::print("Detected Vulkan error: {}", magic_enum::enum_name(err)); \
            abort(); \
        } \
    } while (0)

#endif // DEFINES_H
