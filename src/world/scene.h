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
	inline Scene(std::vector<Chunk> v = {})
		// We need to initialize it, but as we don't have anythin, we use a temporary vector at the start.
		: view(v | std::ranges::views::drop(0) | std::ranges::views::take(0))
	{
	}

	Graphics::Resources *res = nullptr;
	std::vector<Chunk> chunks {};

	std::ranges::take_view<std::ranges::drop_view<std::ranges::ref_view<std::vector<Chunk>>>> view;
};


}


#endif // WORLD_SCENE_H
