#ifndef GRAPHICS_SCENE_H
#define GRAPHICS_SCENE_H

#include "resources.h"
#include "chunk.h"


namespace Graphics
{

class Scene
{
public:
	Resources &res;
	std::vector<Chunk> chunks {};
	std::vector<Mesh> meshes {};
};

}


#endif // GRAPHICS_SCENE_H
