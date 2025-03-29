#ifndef SCENE_H
#define SCENE_H

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


#endif // SCENE_H
