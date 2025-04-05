#ifndef NDEBUG
//#define VMA_USE_DEBUG_LOG
#endif

#ifdef VMA_USE_DEBUG_LOG

#include <string>
#include <cstring>


static int allocationCounter = 0;

template<typename ... Args>
inline void vma_printer(const char *format, Args ... args)
{
	std::string str("VMA LOG: ");
	str += format;
	str += "\n";
	printf(str.c_str(), args...);

	if (strstr(format, "Allocate")) {
		printf("Alloc\n");
		allocationCounter++;
	} else if (strstr(format, "Free")) {
		printf("Dealloc\n");
		allocationCounter--;
	}
}

template<>
inline void vma_printer(const char *format)
{
	std::string str("VMA LOG: ");
	str += format;
	str += "\n";
	printf("%s", str.c_str());

	if (strstr(format, "Allocate")) {
		printf("Alloc\n");
		allocationCounter++;
	} else if (strstr(format, "Free")) {
		printf("Dealloc\n");
		allocationCounter--;
	}
}

#define VMA_DEBUG_LOG(format, ...) \
do { \
	    vma_printer(format, ##__VA_ARGS__); \
} while (false)

#endif

#define VMA_IMPLEMENTATION
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


namespace Graphics
{

void *get_mapped_data(const VmaAllocation alloc)
{
	assert(alloc != VK_NULL_HANDLE);

	return alloc->GetMappedData();
}


}
