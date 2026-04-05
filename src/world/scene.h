#ifndef WORLD_SCENE_H
#define WORLD_SCENE_H

#include <gsl/gsl-lite.hpp>
#include <ranges>
#include <set>

#include "../graphics/resources.h"
#include "../world/objectdata.h"

namespace World {

class Scene
{
public:
    /**
	 * @brief Scene
	 * @param v NEVER SET THE VALUE.
	 *
	 * The argument is used due to the requirement of an std::vector during initialization.
	 * Passing a custom value as parameter has unknown effects.
	 */
    inline explicit Scene(std::vector<Graphics::Chunk2> &v2)
        // We need to initialize it, but as we don't have anythin, we use a temporary vector at the start.
        : view2(v2 | std::ranges::views::drop(0) | std::ranges::views::take(0))
    {}

    /**
     * @brief Resources associated to this scene.
     */
    std::shared_ptr<Graphics::Resources2> res2 = nullptr;
    /**
     * @brief Chunks composing the scene.
     */
    std::vector<Graphics::Chunk2> chunks2{};

    /**
     * @brief Chunk contained in the whole scene.
     * This chunk is used to contain all elements that moves freely through
     * the different classic chunks of the scene. A good example would be the player itself.
     */
    Graphics::Chunk2 movings2{};
    Graphics::ObjectData *player2 = nullptr;

    std::set<std::pair<const Physics::Entity *, const Physics::Entity *>> collisions;

    std::ranges::take_view<std::ranges::drop_view<std::ranges::ref_view<std::vector<Graphics::Chunk2>>>> view2;
};

} // namespace World

#endif // WORLD_SCENE_H
