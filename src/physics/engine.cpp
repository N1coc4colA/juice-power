#include "engine.h"

#include <chrono>


namespace
{

static auto prevChrono = std::chrono::system_clock::now();

}


namespace Physics
{

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
	const auto delta = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevChrono).count()) / 200.0;

	// It's true that sometimes, delta is so small that it's 0, so we have to skip the operation.
	if (delta == 0.) {
		return;
	}

	// Reset collisions data.
	m_scene->collisions.clear();
	m_scene->collisions.resize(m_scene->view.size());

	for (auto &c : m_scene->view) {
		for (auto &obj : c.entities) {
			obj.compute(delta);
		}
	}

	{
		const auto size = m_scene->view.size();

		for (size_t i = 0; i < size; i++) {
			const auto &c = m_scene->view[i];

			// For every entity in the current chunk, we check the entities in the current AND next chunk.
			// Previous chunks' entities have already been checked against.
			for (const auto &e : c.entities) {
				for (size_t j = i; j < size; j++) {
					auto &c2 = m_scene->view[j];

					for (const auto &e2 : c2.entities) {
						Projection proj;

						// If both elements are not the same, we can check for collision.
						if (&e != &e2) {
							[[likely]];

							if (e.collides(e2, proj)) {
								// We need to resolve the collision.
							}
						}
					}
				}
			}
		}
	}

	for (auto &chunk : m_scene->view) {
		const auto size = chunk.entities.size();
		for (size_t i = 0; i < size; i++) {
			chunk.positions[i].x = chunk.entities[i].position.x;
			chunk.positions[i].y = chunk.entities[i].position.y;
		}
	}

	prevChrono = currentTime;
}


}
