#include <iostream>

#include <magic_enum.hpp>

#include "defines.h"
#include "orchestrator.h"

#include "graphics/engine.h"
#include "graphics/resources.h"
#include "loaders/map.h"
#include "world/scene.h"

int main(int argc, char **argv)
{
	UNUSED(argv);
	UNUSED(argc);

    std::vector<Graphics::Chunk2> chunks{};
    World::Scene scene(chunks);
    Orchestrator orchestrator{};

    {
        const auto error = orchestrator.loadMap(scene, "/home/nicolas/Documents/repos/github/juice-power/maps/0");
        if (error != Loaders::Status::Ok) {
            std::cerr << "Erreur: " << magic_enum::enum_name(error) << '\n';
            return 0;
        }
    }

    orchestrator.setScene(scene);

    orchestrator.run();

    scene.res2->cleanup(orchestrator.graphicsEngine());
    orchestrator.cleanup();

    return 0;
}
