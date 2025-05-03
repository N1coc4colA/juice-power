#include "engine.h"

#include <chrono>


namespace Physics
{

static auto prevChrono = std::chrono::system_clock::now();


Engine::Engine()
{
}

void Engine::setScene(World::Scene &scene)
{
	m_scene = &scene;
}

void Engine::prepare()
{
	prevChrono = std::chrono::system_clock::now();
}

void Engine::compute()
{
	const auto currentTime = std::chrono::system_clock::now();
	const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevChrono).count() / 200.0;

	for (auto &c : m_scene->view) {
		for (auto &obj : c.entities) {
			obj.compute(delta);
		}
	}

	for (auto &chunk : m_scene->view) {
		const auto size = chunk.entities.size();
		for (auto i = 0; i < size; i++) {
			chunk.positions[i].x = chunk.entities[i].position.x;
			chunk.positions[i].y = chunk.entities[i].position.y;
		}
	}

	prevChrono = currentTime;
}


}
