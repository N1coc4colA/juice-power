#include <iostream>

#include <magic_enum.hpp>

//#include "vk/engine.h"

#include "world/chunk.h"
#include "graphics/resources.h"
#include "world/scene.h"
#include "loaders/map.h"

#include "graphics/engine.h"


int main(int argc, char **argv)
{
	//Engine engine {};
	Graphics::Engine engine;
	World::Scene scene;
	//engine.scene = &s;
	//res.build(engine);

	engine.init();
	//engine.loadScene();

	Loaders::Map mapLoader("/home/nicolas/Documents/projects/juice-power/maps/0");
	const auto error = mapLoader.load(engine, scene);
	if (error != Loaders::Status::Ok) {
		std::cout << "Erreur: " << magic_enum::enum_name(error) << std::endl;
		return 0;
	}

	engine.run();

	engine.cleanup();

	return 0;
}
