#include "src/physics/engine.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <ranges>

#include <ctrack.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include "src/config.h"
#include "src/input/defines.h"
#include "src/physics/entity.h"
#include "src/states.h"

namespace
{

/// @brief Constant value of 3/4 of Pi.
constexpr double pi3_4 = M_PI_2 + M_PI;
}

constexpr auto epsiloned(const auto &t)
{
    return t;
}

namespace Physics
{

Engine::Engine() = default;

void Engine::setScene(const std::shared_ptr<World::Scene> &scene)
{
    m_scene = scene;
    rebuildWorld();
}

void Engine::rebuildWorld()
{
    m_world = std::make_unique<b2World>(b2Vec2{0.0f, 9.81f * 0.1f});
    m_bodies.clear();

    if (!m_scene) {
        return;
    }

    m_bodies.resize(m_scene->entities.size(), nullptr);

    for (size_t i = 0; i < m_scene->entities.size(); ++i) {
        auto &setup = m_scene->entities.at<Entity::PhysicsSetup>(i);
        auto &cState = m_scene->entities.at<Entity::PhysicsCartesianState>(i);
        auto &constraints = m_scene->entities.at<Entity::PhysicsConstraints>(i);
        auto &bbox = m_scene->entities.at<Entity::AABB>(i);

        b2BodyDef bodyDef;
        bodyDef.type = setup.isNotFixed ? b2_dynamicBody : b2_staticBody;
        bodyDef.position.Set(cState.position.x, cState.position.y);
        bodyDef.angle = m_scene->entities.at<Entity::PhysicsAngularState>(i).angle;
        bodyDef.linearVelocity.Set(cState.velocity.x, cState.velocity.y);
        bodyDef.angularVelocity = m_scene->entities.at<Entity::PhysicsAngularState>(i).angularVelocity;
        bodyDef.fixedRotation = constraints.fixedRotation;

        b2Body *body = m_world->CreateBody(&bodyDef);

        b2PolygonShape dynamicBox;
        const auto size = (bbox.max - bbox.min) * 0.5f;
        dynamicBox.SetAsBox(size.x, size.y);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &dynamicBox;
        fixtureDef.density = setup.isNotFixed ? 1.0f : 0.0f;
        fixtureDef.friction = 0.3f;
        body->CreateFixture(&fixtureDef);

        m_bodies[i] = body;
    }
}

void Engine::syncSceneFromBodies()
{
    for (size_t i = 0; i < m_bodies.size(); ++i) {
        if (!m_bodies[i]) {
            continue;
        }

        const auto pos = m_bodies[i]->GetPosition();
        const auto angle = m_bodies[i]->GetAngle();
        const auto vel = m_bodies[i]->GetLinearVelocity();
        const auto aVel = m_bodies[i]->GetAngularVelocity();

        m_scene->objects[i].position = {pos.x, pos.y, 1.f, 1.f};

        // Not used for now.
        //m_scene->objects[i].transform = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));

        auto &cState = m_scene->entities.at<Entity::PhysicsCartesianState>(i);
        auto &aState = m_scene->entities.at<Entity::PhysicsAngularState>(i);
        cState.position = {pos.x, pos.y};
        cState.velocity = {vel.x, vel.y};
        aState.angle = angle;
        aState.angularVelocity = aVel;
    }
}

void Engine::prepare()
{
    CTRACK;

    if (!m_scene || !m_world || m_scene->entities.empty()) {
        return;
    }

    for (size_t i = 0; i < m_scene->entities.size(); ++i) {
        if (!m_bodies[i]) {
            continue;
        }

        auto &cState = m_scene->entities.at<Entity::PhysicsCartesianState>(i);
        auto &aState = m_scene->entities.at<Entity::PhysicsAngularState>(i);
        m_bodies[i]->SetTransform(b2Vec2{cState.position.x, cState.position.y}, aState.angle);
        m_bodies[i]->SetLinearVelocity(b2Vec2{cState.velocity.x, cState.velocity.y});
        m_bodies[i]->SetAngularVelocity(aState.angularVelocity);
    }
}

void Engine::compute()
{
    CTRACK;

    if (!m_scene || !m_world) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const double delta = std::chrono::duration<double>(now - m_prevChrono).count();
    m_prevChrono = now;

    if (delta <= 0.0) {
        return;
    }

    if (m_inputState != nullptr) {
        updateMainPosition();
    }

    m_world->Step(static_cast<float>(std::min(delta, static_cast<double>(box2dStep))), 8, 3);
    syncSceneFromBodies();
}

void Engine::run(FrameSync &sync, std::atomic<uint64_t> &commands)
{
    m_prevChrono = std::chrono::system_clock::now();

    while (!sync.isStopped() && !(commands & Stop)) {
        sync.waitForFrameRequest();

        if (sync.isStopped() || (commands & Stop)) {
            break;
        }

        // prepare() reads entity state -> Box2D bodies (picks up input).
        // compute() steps the world once and writes Box2D -> entity state.
        // These are now the only two places that touch the scene from this
        // thread, and they're mutually exclusive with graphics via FrameSync.
        compute();

        sync.signalFrameReady();
    }
}

void Engine::setInputState(Input::InnerState &state)
{
    m_inputState = &state;
}

void Engine::updateMainPosition()
{
    if (!m_scene || m_scene->entities.empty() || m_bodies.empty() || !m_bodies[0]) {
        return;
    }

    // Read all four directions once, under lock, into locals.
    // The input thread may flip them between these reads; that's fine,
    // we're sampling a frame's worth of input.
    const auto left = m_inputState->left.get().state;
    const auto right = m_inputState->right.get().state;
    const auto up = m_inputState->up.get().state;
    const auto down = m_inputState->down.get().state;

    b2Vec2 force{0.f, 0.f};
    constexpr float moveForce = 1500.0f;

    if (left) {
        force.x -= moveForce;
    }
    if (right) {
        force.x += moveForce;
    }
    if (up) {
        force.y -= moveForce;
    }
    if (down) {
        force.y += moveForce;
    }

    m_bodies[0]->ApplyForceToCenter(force, true);
}

} // namespace Physics
