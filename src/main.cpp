#include <iostream>

//#include "vk/engine.h"

#include "graphics/chunk.h"
#include "graphics/resources.h"
#include "graphics/scene.h"

#include "graphics/engine.h"


int main(int argc, char **argv)
{
	/*std::vector<glm::vec3> geometry {};
	geometry.push_back({1, 1, 0});
	geometry.push_back({0, 1, 0});
	geometry.push_back({0, 0, 0});
	geometry.push_back({1, 1, 0});
	geometry.push_back({0, 0, 0});
	geometry.push_back({1, 0, 0});

	Graphics::Resources res {};
	res.geometries.push_back(geometry);
	res.geometries.push_back(geometry);
	res.types = 1;

	Graphics::Chunk chunk {};
	chunk.descriptions.push_back(0);
	chunk.descriptions.push_back(0);
	chunk.positions.push_back({0, 0, 6});
	chunk.positions.push_back({0, 0, -6});
	chunk.transforms.push_back(glm::mat4{1});
	chunk.transforms.push_back(glm::mat4{1});

	Graphics::Scene s {
		.res = res
	};
	s.chunks.push_back(chunk);*/

	//Engine engine {};
	Graphics::Engine engine;
	//engine.scene = &s;
	//res.build(engine);

	engine.init();
	//engine.loadScene();

	engine.run();

	engine.cleanup();

	return 0;
}
