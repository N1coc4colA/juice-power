#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <atomic>
#include <string>

#include "loaders/enums.h"

namespace Graphics {
class Engine;
}

namespace Input {
class Engine;
}

namespace Physics {
class Engine;
}

namespace World {
class Scene;
}

class Orchestrator
{
public:
    explicit Orchestrator();
    Orchestrator(const Orchestrator &) = delete;
    Orchestrator(Orchestrator &&) = delete;
    ~Orchestrator();

    auto operator=(const Orchestrator &) -> Orchestrator & = delete;
    auto operator=(Orchestrator &&) -> Orchestrator & = delete;

    static auto get() -> Orchestrator &;

    void init();
    void run();
    void cleanup();

    auto loadMap(World::Scene &scene, const std::string &path) -> std::tuple<Loaders::Status, std::string>;
    void setScene(World::Scene &scene);

    inline auto graphicsEngine() -> Graphics::Engine & { return *m_graphicsEngine; }
    inline auto physicsEngine() -> Physics::Engine & { return *m_physicsEngine; }
    inline auto inputEngine() -> Input::Engine & { return *m_inputEngine; }

private:
    Graphics::Engine *m_graphicsEngine = nullptr;
    Physics::Engine *m_physicsEngine = nullptr;
    Input::Engine *m_inputEngine = nullptr;

    std::atomic<uint64_t> m_commands = 0;

    static Orchestrator *m_instance;
};

#endif // ORCHESTRATOR_H
