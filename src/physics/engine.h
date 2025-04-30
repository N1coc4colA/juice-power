#ifndef PHYSICS_ENGINE_H
#define PHYSICS_ENGINE_H


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

private:
	bool first = true;
	World::Scene *m_scene = nullptr;
};


}


#endif // PHYSICS_ENGINE_H
