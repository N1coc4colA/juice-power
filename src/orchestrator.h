#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <atomic>
#include <memory>
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

/**
 * @brief Coordinates graphics, physics and input subsystems.
 */
class Orchestrator
{
public:
    /**
     * @brief Builds the singleton orchestrator and initializes subsystems.
     * Only one instance can be alive at once in the process.
     **/
    explicit Orchestrator();
    /// @brief Non-copyable singleton owner.
    Orchestrator(const Orchestrator &) = delete;
    /// @brief Non-movable singleton owner.
    Orchestrator(Orchestrator &&) = delete;
    /// @brief Destroys orchestrator-owned resources.
    ~Orchestrator();

    /// @brief Non-assignable singleton owner.
    auto operator=(const Orchestrator &) -> Orchestrator & = delete;
    /// @brief Non-movable singleton owner.
    auto operator=(Orchestrator &&) -> Orchestrator & = delete;

    /// @brief Returns the process-wide orchestrator instance.
    static auto get() -> Orchestrator &;

    /// @brief Initializes core systems.
    void init();
    /// @brief Runs the game/application main loop.
    void run();
    /// @brief Performs graceful shutdown of subsystems.
    void cleanup();

    /// @brief Loads a map into the provided scene.
    auto loadMap(const std::shared_ptr<World::Scene> &scene, const std::string &path) -> std::tuple<Loaders::Status, std::string>;
    /// @brief Sets the active scene for all subsystems.
    void setScene(const std::shared_ptr<World::Scene> &scene);

    /// @brief Returns the graphics engine instance.
    inline auto graphicsEngine() -> std::shared_ptr<Graphics::Engine> { return m_graphicsEngine; }
    /// @brief Returns the physics engine instance.
    inline auto physicsEngine() -> std::shared_ptr<Physics::Engine> { return m_physicsEngine; }
    /// @brief Returns the input engine instance.
    inline auto inputEngine() -> std::shared_ptr<Input::Engine> { return m_inputEngine; }

private:
    /// @brief Graphics subsystem owner.
    std::shared_ptr<Graphics::Engine> m_graphicsEngine = nullptr;
    /// @brief Physics subsystem owner.
    std::shared_ptr<Physics::Engine> m_physicsEngine = nullptr;
    /// @brief Input subsystem owner.
    std::shared_ptr<Input::Engine> m_inputEngine = nullptr;

    /// @brief Shared command bitmask exchanged between worker loops.
    std::atomic<uint64_t> m_commands = 0;

    /// @brief Singleton instance pointer.
    static Orchestrator *m_instance;
};

#endif // ORCHESTRATOR_H
