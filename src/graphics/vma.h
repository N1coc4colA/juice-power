#ifndef VMA_H
#define VMA_H

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


namespace Graphics
{

// Very anoying to not have all definitions separated from the declaration.
// I don't get why they made a whole bunch of opaque types when they could have
// just separated definitions and declarations instead of putting it all stuck
// together in only one ....... header using a macro.
void *get_mapped_data(VmaAllocation alloc);

int getAllocationsCount();
}


#endif // VMA_H
