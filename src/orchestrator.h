#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <thread>

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

    static Orchestrator &get();

    void init();
    void run();
    void cleanup();

    Loaders::Status loadMap(World::Scene &scene, const std::string &path);
    void setScene(World::Scene &scene);

    inline Graphics::Engine &graphicsEngine() { return *m_graphicsEngine; }
    inline Physics::Engine &physicsEngine() { return *m_physicsEngine; }
    inline Input::Engine &inputEngine() { return *m_inputEngine; }

private:
    Graphics::Engine *m_graphicsEngine = nullptr;
    Physics::Engine *m_physicsEngine = nullptr;
    Input::Engine *m_inputEngine = nullptr;

    std::atomic<uint64_t> m_commands = 0;

    static Orchestrator *m_instance;
};

#endif // ORCHESTRATOR_H
