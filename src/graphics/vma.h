#ifndef JP_GRAPHICS_VMA_H
#define JP_GRAPHICS_VMA_H

using VmaAllocation = struct VmaAllocation_T *;

namespace Graphics
{

// Very anoying to not have all definitions separated from the declaration.
// I don't get why they made a whole bunch of opaque types when they could have
// just separated definitions and declarations instead of putting it all stuck
// together in only one ....... header using a macro.
auto getMappedData(VmaAllocation allocation) -> void *;

/**
 * @brief Get currently remaining allocations.
 * If VMA_USE_DEBUG_LOG is defined, the allocations are counted,
 * otherwise they are not tracked and will return 0.
 * @return number of VMA allocations.
 */
auto getAllocationsCount() -> int;
}

#endif // JP_GRAPHICS_VMA_H
