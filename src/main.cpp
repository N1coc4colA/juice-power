#include <iostream>

#include <magic_enum.hpp>

#include "defines.h"

#include "graphics/resources.h"
#include "world/scene.h"
#include "loaders/map.h"

#include "graphics/engine.h"
#include "physics/engine.h"


int main(int argc, char **argv)
{
	UNUSED(argv);
	UNUSED(argc);

	//Engine engine {};
	Physics::Engine pe;
	Graphics::Engine engine;
	World::Scene scene;
	//engine.scene = &s;
	//res.build(engine);

	engine.init();
	//engine.loadScene();

	Loaders::Map mapLoader("/home/nicolas/Documents/projects/juice-power/maps/0");
	const auto error = mapLoader.load(engine, scene);
	if (error != Loaders::Status::Ok) {
		std::cout << "Erreur: " << magic_enum::enum_name(error) << '\n';
		return 0;
	}

	engine.setScene(scene);
	pe.setScene(scene);
	engine.run(
	[&pe]() {
		pe.prepare();
	},
	[&pe]() {
		pe.compute();
	});

	engine.cleanup();

	return 0;
}
