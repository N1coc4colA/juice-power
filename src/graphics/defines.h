#ifndef DEFINES_H
#define DEFINES_H

#include <magic_enum.hpp>

#include "../defines.h"


#define VK_CHECK(x)                                                            \
do {                                                                           \
	VkResult err = x;                                                          \
	if (err) {                                                                 \
		fmt::print("Detected Vulkan error: {}", magic_enum::enum_name(err));   \
		abort();                                                               \
	}                                                                          \
} while (0)


#endif // DEFINES_H
