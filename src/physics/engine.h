#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H

#include <atomic>

#include "../world/scene.h"

namespace Physics
{

class Engine
{
public:
	Engine();

	void setScene(World::Scene &scene);
	void prepare();
    void compute();
    void run(std::atomic<uint64_t> *commands);

protected:
	void resolveCollision(Physics::Entity &a, Physics::Entity &b, const CollisionInfo &info);

private:
	World::Scene *m_scene = nullptr;

	void dump() const;
};


}


#endif // PHYSICS_ENGINE_H
