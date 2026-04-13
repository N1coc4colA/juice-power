#ifndef JP_WORLD_SCENE_H
#define JP_WORLD_SCENE_H

#include <gsl/gsl-lite.hpp>

#include <set>

#include "src/entity/vector.h"
#include "src/graphics/chunk.h"
#include "src/graphics/resources.h"
#include "src/graphics/types.h"

namespace World {

/**
 * @brief Aggregates render and physics data for the active world.
 */
class Scene
{
public:
    /**
	 * @brief Scene
	 * @param v2 NEVER SET THE VALUE.
	 *
	 * The argument is used due to the requirement of an std::vector during initialization.
	 * Passing a custom value as parameter has unknown effects.
	 */
    explicit Scene(std::vector<Graphics::Chunk> &v2) {}

    /**
     * @brief Resources associated to this scene.
     */
    std::shared_ptr<Graphics::Resources> resources = nullptr;

    /// @brief Groups of contiguous object references sharing draw state.
    std::vector<std::span<Graphics::ObjectData>> references{};
    /// @brief Owned object data entries.
    std::vector<Graphics::ObjectData> objects{};
    /// @brief Physics entities associated with objects.
    Entity::VectorTypes<Entity::PhysicsEntity> entities{};

    /// @brief Set of currently colliding entity pairs.
    std::set<std::pair<int, int>> collisions;
};

} // namespace World

#endif // JP_WORLD_SCENE_H
