#ifndef VMA_H
#define VMA_H

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


// Very anoying to not have all definitions separated from the declaration.
// I don't get why they made a whole bunch of opaque types when they could have
// just separated definitions and declarations instead of putting it all stuck
// together in only one ....... header using a macro.
void *getMappedData(VmaAllocation alloc);


#endif // VMA_H
