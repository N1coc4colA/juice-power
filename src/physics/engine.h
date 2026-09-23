#ifndef JP_PHYSICS_ENGINE_H
#define JP_PHYSICS_ENGINE_H

#include <atomic>
#include <memory>
#include <vector>

#include <box2d/box2d.h>

#include "src/frame_sync.h"
#include "src/world/scene.h"

namespace Input {
struct InnerState;
}

namespace Physics
{

/**
 * @brief Runs physics simulation and collision resolution on the active scene.
 */
class Engine
{
    static constexpr float box2dStep = 1.0f / 60.0f;

public:
    /// @brief Constructs an empty physics engine.
    Engine();

    /// @brief Sets the scene to simulate.
    void setScene(const std::shared_ptr<World::Scene> &scene);
    /// @brief Binds external input state used by simulation.
    void setInputState(Input::InnerState &state);
    /// @brief Prepares per-frame transient simulation data.
    void prepare();
    /// @brief Computes one simulation step.
    void compute();
    /// @brief Runs simulation loop until stop command.
    void run(FrameSync &sync, std::atomic<uint64_t> &commands);

private:
    /// @brief Scene currently simulated.
    std::shared_ptr<World::Scene> m_scene = nullptr;
    /// @brief Borrowed input state pointer.
    Input::InnerState *m_inputState = nullptr;
    /// @brief Box2D world used for all scene simulation.
    std::unique_ptr<b2World> m_world = nullptr;
    /// @brief Bodies backing the scene entities.
    std::vector<b2Body *> m_bodies{};

    /// @brief Rebuild the Box2D world to match the current scene.
    void rebuildWorld();
    /// @brief Synchronizes Box2D body state back to scene entity state.
    void syncSceneFromBodies();
    /// @brief Emits debug dump of simulation state.
    void dump() const;

    /// @brief Updates main controlled entity position from input.
    void updateMainPosition();

    decltype(std::chrono::system_clock::now()) m_prevChrono;
};

}

#endif // JP_PHYSICS_ENGINE_H
