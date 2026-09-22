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

static auto prevChrono = std::chrono::system_clock::now();
static Physics::ComputeState computeState{};
constexpr float box2dStep = 1.0f / 60.0f;
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
        m_scene->objects[i].transform = glm::rotate(glm::mat4(1.0f),
                                                    angle,
                                                    glm::vec3(0.0f, 0.0f, 1.0f));

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

class DumpVisitor
{
public:
    void visit(const Entity::PhysicsObjectState &entity,
               const Entity::PhysicsCartesianState &cState,
               const Entity::PhysicsAngularState &aState,
               const Entity::PhysicsForces &forces,
               const Entity::PhysicsSetup &setup,
               const Entity::PhysicsConstraints &constraints) const
    {
        std::cout << "Entity #" << entity.id << '\n';
        std::cout << "\tpos: " << cState.position.x << " " << cState.position.y << '\n';
        std::cout << "\tvel: " << cState.velocity.x << " " << cState.velocity.y << '\n';
        std::cout << "\tacc: " << cState.acceleration.x << " " << cState.acceleration.y << '\n';
        std::cout << "\tangle: " << aState.angle << '\n';
        std::cout << "\tangular vel: " << aState.angularVelocity << '\n';
        /*std::cout << "\tangular acc: " << aState.angularAcceleration << '\n';
        std::cout << "\tforces: " << forces.forces.x << " " << forces.forces.y << '\n';
        std::cout << "\tthrust: " << forces.thrust.x << " " << forces.thrust.y << '\n';
        std::cout << "\tapplied thrust: " << forces.appliedThrust.x << " " << forces.appliedThrust.y << '\n';*/
        std::cout << "\tmass: " << setup.mass << '\n';
        //std::cout << "\tfriction: " << setup.friction << '\n';
        //std::cout << "\tdrag: " << setup.drag << '\n';
        std::cout << "\telasticity: " << setup.elasticity << '\n';
        std::cout << "\tcan collide: " << setup.canCollide << '\n';
        std::cout << "\tis not fixed: " << setup.isNotFixed << '\n';
        /*std::cout << "\tfixed x: " << constraints.fixedX << '\n';
        std::cout << "\tfixed y: " << constraints.fixedY << '\n';
        std::cout << "\tfixed rotation: " << constraints.fixedRotation << '\n';*/
    }
};

void Engine::dump() const
{
    m_scene->entities.visit(DumpVisitor());
}

void Engine::resolveCollision(const int a, const int b, const Entity::CollisionInfo &info)
{
    using requiredFields = TypesSet<Entity::PhysicsSetup, Entity::PhysicsCartesianState>;

    // a must not be fixed.
    assert(m_scene->entities.at<Entity::PhysicsSetup>(a).isNotFixed);

    // [TODO] Resolve according to the mass.

    const auto &[a_Setup, a_cState] = m_scene->entities.at<requiredFields>(a);
    const auto &[b_Setup, b_cState] = m_scene->entities.at<requiredFields>(b);

    const auto velDelta = a_cState.velocity - b_cState.velocity;

    // a and b must not be separating.
    if (const float relVelProj = glm::dot(velDelta, info.normal); relVelProj >= 0.0f) {
        // [FIXME] This may not be appropriate to do so.
        // If they are separating, don't calculate restitution.
        return;
    }

    if (b_Setup.isNotFixed) {
        const float sumInvMass = 1.0f / a_Setup.mass + 1.0f / b_Setup.mass;
        const float impulseScalar = -(1.0f + a_Setup.elasticity) * glm::dot(velDelta, info.normal) / sumInvMass;
        const glm::vec2 impulse = impulseScalar * info.normal;

        a_cState.velocity += impulse / a_Setup.mass;
        b_cState.velocity -= impulse / b_Setup.mass;

        // Position correction to prevent sinking (Penetration Resolution)
        constexpr float percent = 0.8f; // usually 20% to 80%
        constexpr float slop = 0.01f; // usually 0.01 to 0.1
        const glm::vec2 correction = (std::max(info.depth - slop, 0.0f) / sumInvMass) * percent * info.normal;
        a_cState.position += correction / a_Setup.mass;
        b_cState.position -= correction / b_Setup.mass;
    } else {
        const float impulseScalar = -(1.0f + a_Setup.elasticity) * glm::dot(a_cState.velocity, info.normal);
        a_cState.velocity += impulseScalar * info.normal;

        // Position correction to prevent sinking (Penetration Resolution)
        constexpr float percent = 0.8f; // usually 20% to 80%
        constexpr float slop = 0.01f; // usually 0.01 to 0.1
        const glm::vec2 correction = std::max(info.depth - slop, 0.0f) * percent * info.normal;
        a_cState.position += correction;
    }
}

class CollisionReset
{
public:
    void visit(Entity::PhysicsObjectState &entity) const { entity.hasCollision = false; }
};

class ObjectCompute
{
public:
    explicit ObjectCompute(const double timeDelta)
        : timeDelta(timeDelta)
    {}

    void visit(Entity::PhysicsCartesianState &cState,
               Entity::PhysicsAngularState &aState,
               Entity::PhysicsSetup &setup,
               Entity::PhysicsForces &forces,
               Entity::PhysicsConstraints &constraints) const
    {
        Physics::compute(timeDelta, ReferencesSet{cState, aState, setup, forces, constraints});
    }

    double timeDelta;
};

void Engine::compute()
{
    CTRACK;

    if (!m_scene || !m_world) {
        return;
    }

    const auto currentTime = std::chrono::system_clock::now();
    const auto delta = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevChrono).count()) / 1000.0;

    if (delta <= 0.0) {
        return;
    }

    if (m_inputState != nullptr) {
        updateMainPosition();
    }

    m_world->Step(static_cast<float>(std::min(delta, static_cast<double>(box2dStep))), 8, 3);
    syncSceneFromBodies();

    prevChrono = currentTime;
}

void Engine::collisionResolutionFilter(const int a, const int b)
{
    if (a == b) {
        return;
    }

    const std::pair m = {std::min(a, b), std::max(a, b)};
    if (m_scene->collisions.contains(m)) {
        return;
    }

    const auto argsA = m_scene->entities.at<RemoveConstReferencesType<Physics::ComputeState::CollisionParameters>>(a);
    const auto argsB = m_scene->entities.at<RemoveConstReferencesType<Physics::ComputeState::CollisionParameters>>(b);

    const auto &aSetup = std::get<0>(argsA);
    const auto &bSetup = std::get<0>(argsB);

    const bool mayCollide = aSetup.canCollide || bSetup.canCollide;
    const bool mayNotBeFixed = aSetup.isNotFixed || bSetup.isNotFixed;

    if (!(mayCollide && mayNotBeFixed)) {
        return;
    }

    // We need to resolve the collision.

    if (Entity::CollisionInfo info{}; computeState.collides(argsA, argsB, info)) {
        //std::cout << "Detected collision between entities " << e.id << " and " << e2.id << " normal=(" << info.normal.x << "," << info.normal.y << ") depth=" << info.depth << "\n";
        resolveCollision(a, b, info);

        std::get<1>(argsA).hasCollision = aSetup.canCollide;
        std::get<1>(argsB).hasCollision = bSetup.canCollide;

        m_scene->collisions.insert(m);
    }
}

void Engine::resolveAllCollisions()
{
    const auto size = static_cast<int64_t>(m_scene->entities.size());

    for (int i = 0; i < size; i++) {
        // For every entity in the current chunk, we check the entities in the current AND next chunk.
        // Previous chunks' entities have already been checked against.
        for (int j = i + 1; j < size; j++) {
            collisionResolutionFilter(i, j);
        }
    }
}

void Engine::run(std::atomic<uint64_t> &commands)
{
    while (!(commands & Stop)) {
        if (commands & PrepareDrawing) {
            prepare();
            commands |= DrawingPrepared;
        }

        compute();
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

    b2Vec2 force{0.0f, 0.0f};
    constexpr float moveForce = 1500.0f;

    if (m_inputState->left.unsafeGet().state) {
        force.x -= moveForce;
    }
    if (m_inputState->right.unsafeGet().state) {
        force.x += moveForce;
    }
    if (m_inputState->up.unsafeGet().state) {
        force.y -= moveForce;
    }
    if (m_inputState->down.unsafeGet().state) {
        force.y += moveForce;
    }

    m_bodies[0]->ApplyForceToCenter(force, true);
}

} // namespace Physics
