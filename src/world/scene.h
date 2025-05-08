#ifndef WORLD_SCENE_H
#define WORLD_SCENE_H

#include <ranges>

#include "chunk.h"

#include "../graphics/resources.h"


namespace World
{

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
	inline explicit Scene(std::vector<Chunk> v = {})
		// We need to initialize it, but as we don't have anythin, we use a temporary vector at the start.
		: view(v | std::ranges::views::drop(0) | std::ranges::views::take(0))
	{
	}

	Graphics::Resources *res = nullptr;
	std::vector<Chunk> chunks {};

	std::vector<std::pair<int, std::vector<Physics::Entity *>>> collisions {};

	std::ranges::take_view<std::ranges::drop_view<std::ranges::ref_view<std::vector<Chunk>>>> view;
};


}


#endif // WORLD_SCENE_H
