#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include <atomic>

#include "../world/scene.h"

namespace Input {
struct InnerState;
}

namespace Physics
{

class Engine
{
public:
	Engine();

	void setScene(World::Scene &scene);
    void setInputState(Input::InnerState &state);
    void prepare();
    void compute();
    void run(std::atomic<uint64_t> &commands);

protected:
	void resolveCollision(Physics::Entity &a, Physics::Entity &b, const CollisionInfo &info);
    void resolveAllCollisions();
    void resolveCollisions(std::vector<Physics::Entity> &en1, Physics::Entity &e);

private:
    World::Scene *m_scene = nullptr;
    Input::InnerState *m_inputState = nullptr;

    void dump() const;

    void updateMainPosition();
};


}


#endif // PHYSICS_ENGINE_H
