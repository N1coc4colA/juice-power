#include <iostream>

#include <magic_enum.hpp>

#include <ctrack.hpp>

#include "src/defines.h"
#include "src/graphics/engine.h"
#include "src/loaders/map.h"
#include "src/orchestrator.h"
#include "src/threadpool.h"
#include "src/world/scene.h"

auto main(const int argc, char **argv) -> int
{
	unused(argv);
	unused(argc);

    ThreadPool threadPool{};

    std::vector<Graphics::Chunk> chunks{};
    auto scene = std::make_shared<World::Scene>(chunks);
    Orchestrator orchestrator{};

    if (const auto error = orchestrator.loadMap(scene, "/home/nicolas/Documents/repos/github/juice-power/maps/0"); std::get<0>(error) != Loaders::Status::Ok) {
        std::cerr << "Erreur: " << magic_enum::enum_name(std::get<0>(error)) << ": " << std::get<1>(error) << '\n';
        return 0;
    }

    orchestrator.setScene(scene);

    orchestrator.run();

    scene->resources->cleanup(orchestrator.graphicsEngine());
    orchestrator.cleanup();

    ctrack::result_print();

    return 0;
}
