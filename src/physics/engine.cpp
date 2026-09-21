#include "src/physics/engine.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <ranges>

#include <ctrack.hpp>

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
constexpr float kBox2DStep = 1.0f / 60.0f;
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
        bodyDef.angle = setup.angle;
        bodyDef.linearVelocity.Set(cState.velocity.x, cState.velocity.y);
        bodyDef.angularVelocity = m_scene->entities.at<Entity::PhysicsAngularState>(i).angularVelocity;
        bodyDef.gravityScale = setup.isNotFixed ? 1.0f : 0.0f;

        auto *body = m_world->CreateBody(&bodyDef);
        if (body == nullptr) {
            continue;
        }

        const auto half = (bbox.max - bbox.min) * 0.5f;
        const auto halfX = std::max(half.x, 0.5f);
        const auto halfY = std::max(half.y, 0.5f);

        b2PolygonShape shape;
        shape.SetAsBox(halfX, halfY);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &shape;
        fixtureDef.density = std::max(1.0f, setup.mass > 0.0f ? 1.0f / setup.mass : 1.0f);
        fixtureDef.friction = constraints.friction;
        fixtureDef.restitution = setup.elasticity;
        body->CreateFixture(&fixtureDef);

        body->SetFixedRotation(false);
        m_bodies[i] = body;
    }
}

void Engine::syncSceneFromBodies()
{
    if (!m_scene || !m_world || m_scene->entities.empty()) {
        return;
    }

    for (size_t i = 0; i < m_scene->entities.size(); ++i) {
        auto *body = m_bodies.at(i);
        if (body == nullptr) {
            continue;
        }

        auto &cState = m_scene->entities.at<Entity::PhysicsCartesianState>(i);
        auto &aState = m_scene->entities.at<Entity::PhysicsAngularState>(i);

        const auto bodyPosition = body->GetPosition();
        cState.position = {bodyPosition.x, bodyPosition.y};
        cState.velocity = {body->GetLinearVelocity().x, body->GetLinearVelocity().y};
        aState.angularVelocity = body->GetAngularVelocity();
    }
}

void Engine::setInputState(Input::InnerState &state)
{
    m_inputState = &state;
}

void Engine::prepare()
{
	prevChrono = std::chrono::system_clock::now();
}

class DumpVisitor
{
public:
    void visit(const Entity::PhysicsSetup &setup,
               const Entity::PhysicsObjectState &objState,
               const Entity::AABB &boundingBox,
               const Entity::PhysicsConstraints &constraints,
               const Entity::PhysicsCartesianState &cState,
               const Entity::PhysicsAngularState &aState) const
    {
        const auto center = Physics::center(cState, boundingBox);

        std::cout << "id: " << objState.id << ", ";
        std::cout << "position: (" << cState.position.x << ", " << cState.position.y << "), ";
        std::cout << "velocity: (" << cState.velocity.x << ", " << cState.velocity.y << "), ";
        std::cout << "acceleration: (" << cState.acceleration.x << ", " << cState.acceleration.y << "), ";
        std::cout << "angular_velocity: " << aState.angularVelocity << ", ";
        std::cout << "elasticity: " << setup.elasticity << ", mass: " << setup.mass << ", " << ", angle: " << setup.angle;
        std::cout << "canCollide: " << setup.canCollide << ", isNotFixed: " << setup.isNotFixed << ", ";
        std::cout << "MoI: " << constraints.MoI << ", ";
        std::cout << "bounding_box_min: (" << boundingBox.min.x << ", " << boundingBox.min.y << "), ";
        std::cout << "bounding_box_max: (" << boundingBox.max.x << ", " << boundingBox.max.y << ")\n";
    }
};

void Engine::dump() const
{
    m_scene->entities.visit(DumpVisitor());
}

constexpr auto rotate(const glm::vec2 &v) noexcept -> glm::vec2
{
	return glm::vec2{-v.y, v.x};
}

constexpr auto rotate(const glm::vec2 &v, const double angle) -> glm::vec2
{
	const auto c = glm::cos(angle);
	const auto s = glm::sin(angle);

	return glm::vec2 {v.x * c - v.y * s, v.x * s + v.y * c};
}

void Engine::resolveCollision(const int a, const int b, const Entity::CollisionInfo &info)
{
    using requiredFields = ReferencesSet<Entity::PhysicsSetup, Entity::PhysicsCartesianState>;

    if (info.depth < Config::physicsEpsilon) {
        return;
    }

    const auto &[a_Setup, a_cState] = m_scene->entities.at<requiredFields>(a);
    const auto &[b_Setup, b_cState] = m_scene->entities.at<requiredFields>(b);

    // Relative velocity
    const glm::vec2 relVel = a_cState.velocity - b_cState.velocity;

    // Velocity along the normal
    const float velAlongNormal = glm::dot(relVel, info.normal);

    // If separating and no penetration, skip
    if (velAlongNormal > Config::physicsEpsilon && info.depth <= Config::physicsEpsilon) {
        return;
    }

    // Restitution (elasticity)
    const float e = std::clamp(std::min(a_Setup.elasticity, b_Setup.elasticity), 0.0f, 1.0f);

    // Inverse masses
    const float invMassA = a_Setup.isNotFixed ? 1.0f / a_Setup.mass : 0.0f;
    const float invMassB = b_Setup.isNotFixed ? 1.0f / b_Setup.mass : 0.0f;

    const float denom = epsiloned(invMassA + invMassB);
    if (denom == 0.f) {
        return; // both infinite mass
    }

    // Impulse scalar (normal)
    const float j = -(1.0f + e) * velAlongNormal * 0.95f / denom;

    const glm::vec2 impulse = j * info.normal;

    /*
        a b choice
        0 0 1+2 = 3 // Don't care.
        0 1 1+4 = 5
        1 0 2+2 = 4
        1 1 2+4 = 6
    */
    switch ((1 << a_Setup.canCollide) + (2 << b_Setup.canCollide)) {
    case 4: {
        const auto v = epsiloned(impulse * invMassA);

        a_cState.velocity += v;
        a_cState.position -= info.normal * info.depth;
        break;
    }
    case 5: {
        const auto v = epsiloned(impulse * invMassB);

        b_cState.velocity -= v;
        b_cState.position += info.normal * info.depth;
        break;
    }
    case 6: {
        a_cState.velocity += epsiloned(impulse * invMassA);
        b_cState.velocity -= epsiloned(impulse * invMassB);
        break;
    }
    case 3: {
        break;
    }
    default: {
        assert(false && "Entering this switch case should never have happened.");
        break;
    }
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

    m_world->Step(static_cast<float>(std::min(delta, static_cast<double>(kBox2DStep))), 8, 3);
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

    const auto argsA = m_scene->entities.at<removeConstReferencesType<Physics::ComputeState::CollisionParameters>>(a);
    const auto argsB = m_scene->entities.at<removeConstReferencesType<Physics::ComputeState::CollisionParameters>>(b);

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
        compute();

        if (commands & PrepareDrawing) {
            const auto pStateRange = m_scene->entities.range<Entity::PhysicsCartesianState>();
            for (auto &&[obj, entity] : std::views::zip(m_scene->objects, pStateRange)) {
                obj.position = glm::vec4(std::get<0>(entity).position, 0.f, 1.f);
            }

            // Update states.
            commands &= ~PrepareDrawing;
            commands |= DrawingPrepared;
        }
    }
}

void Engine::updateMainPosition()
{
    if (!m_scene || !m_world || m_scene->entities.empty() || m_inputState == nullptr) {
        return;
    }

    auto *body = m_bodies.empty() ? nullptr : m_bodies.front();
    if (body == nullptr) {
        return;
    }

    constexpr float forceMagnitude = 40.0f;

    if (m_inputState->left.unsafeGet().state) {
        body->ApplyForceToCenter({-forceMagnitude, 0.0f}, true);
    }
    if (m_inputState->right.unsafeGet().state) {
        body->ApplyForceToCenter({forceMagnitude, 0.0f}, true);
    }
    if (m_inputState->down.unsafeGet().state) {
        body->ApplyForceToCenter({0.0f, forceMagnitude}, true);
    }
    if (m_inputState->up.unsafeGet().state) {
        body->ApplyForceToCenter({0.0f, -forceMagnitude}, true);
    }
}
}
