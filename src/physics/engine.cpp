
#include "engine.h"

#include <chrono>
#include <iostream>


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

inline constexpr float cross(const glm::vec2 &a, const glm::vec2 &b) noexcept
{
	return a.x * b.y - a.y * b.x;
}

inline constexpr glm::vec2 rotate(const glm::vec2 &v) noexcept
{
	return glm::vec2{-v.y, v.x};
}

void Engine::resolveCollision(Physics::Entity &a, Physics::Entity &b, const CollisionInfo &info)
{
	// Relative points
	const auto ca = info.point - a.center();
	const auto cb = info.point - b.center();

	// Calculate relative velocity
	const auto rotVelA = rotate(ca) * a.angularVelocity;
	const auto rotVelB = rotate(cb) * b.angularVelocity;
	const auto velRel = b.velocity + rotVelB - a.velocity + rotVelA;

	const float velocityOnNormal = glm::dot(velRel, info.normal);
	if (velocityOnNormal < 0.f) {
		return;
	}

	const float elasticity = std::min(a.elasticity, b.elasticity);
	const float rotA = cross(rotVelA, info.normal);
	const float rotB = cross(rotVelB, info.normal);

	const float denominator = (1.f / a.mass) + (1.f / b.mass) +
							  rotA*rotA / a.MoI + rotB*rotB / b.MoI;

	// Calculate impulse
	const float j = -(1.f + elasticity) * velocityOnNormal / denominator;
	const auto impulse = j * info.normal;

	// Update values
	if (a.isNotFixed) {
		a.velocity -= impulse / a.mass;
		a.angularVelocity -= cross(ca, impulse) / a.MoI;
	}

	if (b.isNotFixed) {
		b.velocity += impulse / b.mass;
		b.angularVelocity += cross(cb, impulse) / b.MoI;
	}

	// [NOTE] Also use tangential impulse ?
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

									dump();
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

	/*for (auto &chunk : m_scene->view) {
		const auto size = chunk.entities.size();
		for (size_t i = 0; i < size; i++) {
			std::cout << i << ':' << chunk.positions[i].x << ':' << chunk.positions[i].y << '\n';
		}
	}*/

	prevChrono = currentTime;
}


}
