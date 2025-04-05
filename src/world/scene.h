#ifndef GRAPHICS_SCENE_H
#define GRAPHICS_SCENE_H

#include "../graphics/resources.h"
#include "chunk.h"


namespace Graphics
{

class Scene
{
public:
	Resources &res;
	std::vector<Chunk> chunks {};
};


}


#endif // GRAPHICS_SCENE_H
