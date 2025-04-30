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

	for (auto &chunk : scene.chunks) {
		chunk.entities.resize(chunk.positions.size());
	}

	for (auto &chunk : scene.chunks) {
		const auto size = chunk.entities.size();
		for (auto i = 0; i < size; i++) {
			chunk.entities[i].position = chunk.positions[i];
		}
	}

	for (auto &chunk : scene.chunks) {
		const auto size = chunk.entities.size();
		for (auto i = 0; i < size; i++) {
			std::vector<glm::vec2> borders {};
			borders.reserve(4);

			//chunk.entities[i].borders = scene.res->vertices[chunk.descriptions[i]];
		}
	}
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
