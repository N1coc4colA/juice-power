#ifndef SCENE_H
#define SCENE_H

#include "resources.h"
#include "chunk.h"


class Scene
{
public:
	Resources *res;
	std::vector<Chunk> chunks {};
};


#endif // SCENE_H
