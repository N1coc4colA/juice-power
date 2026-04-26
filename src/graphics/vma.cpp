#include "src/graphics/vma.h"

#ifndef NDEBUG
//#define VMA_USE_DEBUG_LOG
#endif

#ifdef VMA_USE_DEBUG_LOG

#include <cstring>
#include <print>
#include <string>

static int allocationCounter = 0;

template<typename... Args>
inline void vmaPrinter(const char *format, Args... args)
{
	std::string str("VMA LOG: ");
	str += format;
	str += "\n";
	printf(str.c_str(), args...);

	if (strstr(format, "Allocate")) {
        std::println("Alloc");
        allocationCounter++;
    } else if (strstr(format, "Free")) {
        std::println("Dealloc\n");
        allocationCounter--;
    }
}

template<>
inline void vmaPrinter(const char *format)
{
	std::string str("VMA LOG: ");
	str += format;
	str += "\n";
    std::print("{}", str);

    if (strstr(format, "Allocate")) {
        std::println("Alloc");
        allocationCounter++;
    } else if (strstr(format, "Free")) {
        std::println("Dealloc");
        allocationCounter--;
    }
}

#define VMA_DEBUG_LOG(format, ...) \
    do { \
        vmaPrinter(format, ##__VA_ARGS__); \
    } while (false)

#endif

#define VMA_IMPLEMENTATION
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


namespace Graphics
{

auto getMappedData(const VmaAllocation allocation) -> void *
{
    assert(allocation != VK_NULL_HANDLE);

    return allocation->GetMappedData();
}

auto getAllocationsCount() -> int
{
#ifdef VMA_USE_DEBUG_LOG
    return allocationCounter;
#else
    return 0;
#endif
}
}
