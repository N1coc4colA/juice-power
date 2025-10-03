#include "engine.h"

#include <chrono>
#include <iostream>

#include "src/input/defines.h"

//#include "src/helpers.h"
#include "src/states.h"

namespace
{

static auto prevChrono = std::chrono::system_clock::now();
constexpr double pi3_4 = M_PI_2 + M_PI;

}

inline constexpr auto epsiloned(const auto &t)
{
    return t;
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

void Engine::setInputState(Input::InnerState &state)
{
    m_inputState = &state;
}

void Engine::prepare()
{
	prevChrono = std::chrono::system_clock::now();
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
    if (info.depth < 0.00001f) {
        return;
    }

    // Relative velocity
    const glm::vec2 relVel = a.velocity - b.velocity;

    // Velocity along the normal
    const float velAlongNormal = glm::dot(relVel, info.normal);

    // If separating and no penetration, skip
    if (velAlongNormal > 0.00001f && info.depth <= 0.00001f) {
        return;
    }

    // Restitution (elasticity)
    const float e = std::clamp(std::min(a.elasticity, b.elasticity), 0.0f, 1.0f);

    // Inverse masses
    const float invMassA = a.isNotFixed ? 1.0f / a.mass : 0.0f;
    const float invMassB = b.isNotFixed ? 1.0f / b.mass : 0.0f;

    const float denom = epsiloned(invMassA + invMassB);
    if (denom == 0.f) {
        return; // both infinite mass
    }

    // Impulse scalar (normal)
    const float j = -(1.0f + e) * velAlongNormal * 0.95f / denom;

    const glm::vec2 impulse = j * info.normal;

    if (a.canCollide && b.canCollide) {
        {
            const auto v = epsiloned(impulse * invMassA);

            a.velocity += v;
        }
        {
            const auto v = epsiloned(impulse * invMassB);

            b.velocity -= v;
        }
    } else if (a.canCollide) {
        const auto v = epsiloned(impulse * invMassA);

        a.velocity += v;
        a.position -= info.normal * info.depth;
    } else if (b.canCollide) {
        const auto v = epsiloned(impulse * invMassB);

        b.velocity -= v;
        b.position += info.normal * info.depth;
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

    /* Resolve collisions */ {
        m_scene->collisions.clear();

        for (auto &c : m_scene->view) {
            for (auto &e : c.entities) {
                e.has_collision = false;
            }
        }
        for (auto &e : m_scene->movings.entities) {
            e.has_collision = false;
        }

        resolveAllCollisions();
    }

    /* Cleaning up */ {
        for (auto &c : m_scene->view) {
            for (auto &obj : c.entities) {
                obj.cleanup();
            }
        }
        for (auto &obj : m_scene->movings.entities) {
            obj.cleanup();
        }
    }

    /* Position update */ {
        for (auto &c : m_scene->view) {
            for (auto &obj : c.entities) {
                obj.compute(delta);
            }
        }
        for (auto &obj : m_scene->movings.entities) {
            obj.compute(delta);
        }

        updateMainPosition();
    }

    prevChrono = currentTime;
}

void Engine::resolveCollisions(std::vector<Physics::Entity> &en1, Physics::Entity &e)
{
    for (auto &e2 : en1) {
        CollisionInfo info{};

        // If both elements are not the same, we can check for collision.
        const std::pair m = {std::min(&e, &e2), std::max(&e, &e2)};
        if (&e != &e2) {
            [[likely]];

            if (!m_scene->collisions.contains(m)) {
                if ((e.canCollide || e2.canCollide) && (e.isNotFixed || e2.isNotFixed)) {
                    // We need to resolve the collision.
                    if (e.collides(e2, info)) {
                        resolveCollision(e, e2, info);

                        if (e.canCollide) {
                            e.has_collision = true;
                        }
                        if (e2.canCollide) {
                            e2.has_collision = true;
                        }

                        m_scene->collisions.insert(m);
                    }
                }
            }
        }
    }
}

void Engine::resolveAllCollisions()
{
    {
        const auto size = m_scene->view.size();

        for (size_t i = 0; i < size; i++) {
            // For every entity in the current chunk, we check the entities in the current AND next chunk.
            // Previous chunks' entities have already been checked against.
            for (auto &e : m_scene->view[i].entities) {
                for (size_t j = i; j < size; j++) {
                    auto &c2 = m_scene->view[j];

                    resolveCollisions(c2.entities, e);
                }
            }
        }

        for (auto &c : m_scene->view) {
            for (auto &e : c.entities) {
                resolveCollisions(m_scene->movings.entities, e);
            }
        }

        for (auto &e : m_scene->movings.entities) {
            resolveCollisions(m_scene->movings.entities, e);
        }
    }
}

void copyPositions(World::Chunk &c)
{
    const auto size = c.entities.size();
    for (size_t i = 0; i < size; ++i) {
        c.positions[i] = glm::vec3(c.entities[i].position, 0.f);
    }
}

void Engine::run(std::atomic<uint64_t> &commands)
{
    while (!(commands & CommandStates::Stop)) {
        compute();

        if (commands & CommandStates::PrepareDrawing) {
            copyPositions(m_scene->movings);
            for (auto &chunk : m_scene->view) {
                copyPositions(chunk);
            }

            // Update states.
            commands &= ~CommandStates::PrepareDrawing;
            commands |= CommandStates::DrawingPrepared;
        }
    }
}

void Engine::updateMainPosition()
{
    constexpr auto horVel = 0.05f;
    constexpr auto vertVel = 0.01f;

    if (m_inputState->left.unsafeGet().state) {
        std::cout << "left" << std::endl;
        //m_scene->movings.entities[0].velocity.x += horVel;
        m_scene->movings.entities[0].thrusts.push_back(Thrust{.vector = {horVel, 0.f, 0.f}});
    }
    if (m_inputState->right.unsafeGet().state) {
        std::cout << "right" << std::endl;
        //m_scene->movings.entities[0].velocity.x += horVel;
        m_scene->movings.entities[0].thrusts.push_back(Thrust{.vector = {-horVel, 0.f, 0.f}});
    }
    if (m_inputState->down.unsafeGet().state) {
        std::cout << "down" << std::endl;
        m_scene->movings.entities[0].velocity.x -= vertVel;
    }
    if (m_inputState->up.unsafeGet().state && !m_inputState->up.unsafeGet().hold) {
        std::cout << "up" << std::endl;
        m_scene->movings.entities[0].velocity.x += vertVel;
    }
}
}
