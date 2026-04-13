#ifndef JP_PHYSICS_ENGINE_H
#define JP_PHYSICS_ENGINE_H

#include <atomic>

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
    void run(std::atomic<uint64_t> &commands);

protected:
    /// @brief Resolves contact between two entities.
    void resolveCollision(int a, int b, const Entity::CollisionInfo &info);
    void collisionResolutionFilter(int a, int b);
    /// @brief Resolves all currently detected collisions.
    void resolveAllCollisions();

private:
    /// @brief Scene currently simulated.
    std::shared_ptr<World::Scene> m_scene = nullptr;
    /// @brief Borrowed input state pointer.
    Input::InnerState *m_inputState = nullptr;

    /// @brief Emits debug dump of simulation state.
    void dump() const;

    /// @brief Updates main controlled entity position from input.
    void updateMainPosition();
};

}

#endif // JP_PHYSICS_ENGINE_H
