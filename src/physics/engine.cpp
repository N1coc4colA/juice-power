#include "engine.h"

#include <chrono>
#include <iostream>

#include "src/utils.h"

namespace
{

static auto prevChrono = std::chrono::system_clock::now();
constexpr double pi3_4 = M_PI_2 + M_PI;

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

	//dump();
}

void Engine::dump() const
{
	const auto csize = m_scene->chunks.size();

	for (size_t i = 0; i < csize; i++) {
		const auto &c = m_scene->chunks[i];

		const auto esize = c.entities.size();
		for (size_t j = 0; j < esize; j++) {
			const auto &e = c.entities[j];

			const auto center = e.center();

			std::cout << "id: " << i*10+j << ", ";
			std::cout << "position: (" << e.position.x << ", " << e.position.y << "), ";
			std::cout << "center: (" << center.x << ", " << center.y << "), ";
			std::cout << "velocity: (" << e.velocity.x << ", " << e.velocity.y << "), ";
			std::cout << "acceleration: (" << e.acceleration.x << ", " << e.acceleration.y << "), ";
			std::cout << "angular_velocity: " << e.angularVelocity << ", elasticity: " << e.elasticity << ", ";
			std::cout << "MoI: " << e.MoI << ", mass: " << e.mass << ", angle: " << e.angle << ", ";
			std::cout << "canCollide: " << e.canCollide << ", isNotFixed: " << e.isNotFixed << ", ";
			std::cout << "bounding_box_min: (" << e.boundingBox.min.x << ", " << e.boundingBox.min.y << "), ";
			std::cout << "bounding_box_max: (" << e.boundingBox.max.x << ", " << e.boundingBox.max.y << ")\n";
		}
	}
}

inline constexpr glm::vec2 rotate(const glm::vec2 &v) noexcept
{
	return glm::vec2{-v.y, v.x};
}

inline constexpr glm::vec2 rotate(const glm::vec2 &v, const double angle)
{
	const auto c = glm::cos(angle);
	const auto s = glm::sin(angle);

	return glm::vec2 {v.x * c - v.y * s, v.x * s + v.y * c};
}

void Engine::resolveCollision(Physics::Entity &a, Physics::Entity &b, const CollisionInfo &info)
{
	const glm::vec2 comToPointA {};
	const glm::vec2 comToPointB {};

	const auto rotSpeedA = glm::length(comToPointA);
	const auto rotSpeedB = glm::length(comToPointB);

	const auto perpRotVectA = rotate(comToPointA, pi3_4);
	const auto perpRotVectB = rotate(comToPointB, pi3_4);

	const auto rotVectA = perpRotVectA * rotSpeedA;
	const auto rotVectB = perpRotVectB * rotSpeedB;

	const auto relPoint = (a.velocity + rotVectA) - (b.velocity + rotVectB);

	const auto distVect = b.center() - a.center();
	const auto perpImpVect = distVect / glm::length(distVect);

    const auto termA = glm::sqrt(utils::cross(comToPointA, perpImpVect)) / a.MoI;
    const auto termB = glm::sqrt(utils::cross(comToPointB, perpImpVect)) / b.MoI;

    const float elasticity = std::max(a.elasticity, b.elasticity);

    const auto impulse = glm::dot(relPoint, perpImpVect) * -(1.f + elasticity) / (1.f / a.mass + 1.f / b.mass + termA + termB);
	const auto perpImpVectImp = perpImpVect * impulse;

	if (b.isNotFixed) {
		b.velocity -= perpImpVectImp / b.mass;
        b.angularVelocity -= utils::cross(comToPointB, perpImpVectImp) / b.MoI;
    }

	if (a.isNotFixed) {
		a.velocity += perpImpVectImp / a.mass;
        a.angularVelocity += utils::cross(comToPointA, perpImpVectImp) / a.MoI;
    }
}

void Engine::compute()
{
	const auto currentTime = std::chrono::system_clock::now();
	const auto delta = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevChrono).count()) / 400.0; // 200.0

	// It's true that sometimes, delta is so small that it's 0, so we have to skip the operation.
	if (delta == 0.) {
		return;
	}

	for (auto &c : m_scene->view) {
		for (auto &obj : c.entities) {
			obj.cleanup();
		}
	}

	for (auto &c : m_scene->view) {
		for (auto &obj : c.entities) {
			obj.compute(delta);
		}
	}

	// Reset collisions data.
	m_scene->collisions.clear();

	{
		const auto size = m_scene->view.size();

		for (size_t i = 0; i < size; i++) {
			auto &c = m_scene->view[i];

			// For every entity in the current chunk, we check the entities in the current AND next chunk.
			// Previous chunks' entities have already been checked against.
			for (auto &e : c.entities) {
				for (size_t j = i; j < size; j++) {
					auto &c2 = m_scene->view[j];

					for (auto &e2 : c2.entities) {
						CollisionInfo info {};

						// If both elements are not the same, we can check for collision.
						const auto m = std::min(&e, &e2);
						if (&e != &e2 && (!m_scene->collisions.contains(m) || !m_scene->collisions[m].contains(std::max(&e, &e2)))) {
							[[likely]];

							if ((e.canCollide || e2.canCollide) && (e.isNotFixed || e2.isNotFixed)) {
								// We need to resolve the collision.
								if (e.collides(e2, info)) {
									resolveCollision(e, e2, info);
                                }
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
