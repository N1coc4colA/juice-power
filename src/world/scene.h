#ifndef WORLD_SCENE_H
#define WORLD_SCENE_H

#include "../graphics/resources.h"
#include "chunk.h"


namespace World
{

class Scene
{
public:
	Graphics::Resources *res = nullptr;
	std::vector<Chunk> chunks {};
};


}


#endif // WORLD_SCENE_H
