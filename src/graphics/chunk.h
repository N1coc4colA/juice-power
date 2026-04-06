#ifndef CHUNK_H
#define CHUNK_H

#include <vector>

#include "src/graphics/types.h"
#include "src/physics/entity.h"

namespace Graphics {

/**
 * @brief Runtime grouping of drawable object spans and physics entities.
 */
class Chunk
{
public:
    /// @brief Groups of contiguous object references sharing draw state.
    std::vector<std::span<ObjectData>> references{};
    /// @brief Owned object data entries.
    std::vector<ObjectData> objects{};
    /// @brief Physics entities associated with objects.
    std::vector<Physics::Entity> entities{};
};

} // namespace Graphics

#endif // CHUNK_H
