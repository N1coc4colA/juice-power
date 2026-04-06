#ifndef WORLD_SCENE_H
#define WORLD_SCENE_H

#include <gsl/gsl-lite.hpp>

#include <ranges>
#include <set>

#include "src/graphics/chunk.h"
#include "src/graphics/resources.h"

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
    explicit Scene(std::vector<Graphics::Chunk> &v2)
        // We need to initialize it, but as we don't have anythin, we use a temporary vector at the start.
        : view(v2 | std::ranges::views::drop(0) | std::ranges::views::take(0))
    {}

    /**
     * @brief Resources associated to this scene.
     */
    std::shared_ptr<Graphics::Resources> resources = nullptr;
    /**
     * @brief Chunks composing the scene.
     */
    std::vector<Graphics::Chunk> chunks{};

    /**
     * @brief Chunk contained in the whole scene.
     * This chunk is used to contain all elements that moves freely through
     * the different classic chunks of the scene. A good example would be the player itself.
     */
    Graphics::Chunk movings{};
    /// @brief Pointer to the main player object data in scene storage.
    Graphics::ObjectData *player2 = nullptr;

    /// @brief Set of currently colliding entity pairs.
    std::set<std::pair<const Physics::Entity *, const Physics::Entity *>> collisions;

    /// @brief View over active chunks used by iteration logic.
    std::ranges::take_view<std::ranges::drop_view<std::ranges::ref_view<std::vector<Graphics::Chunk>>>> view;
};

} // namespace World

#endif // WORLD_SCENE_H
