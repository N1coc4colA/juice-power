#include "src/physics/engine.h"

#include <chrono>
#include <iostream>
#include <ranges>

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
	const auto currentTime = std::chrono::system_clock::now();
	const auto delta = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevChrono).count()) / 400.0; // 200.0

	// It's true that sometimes, delta is so small that it's 0, so we have to skip the operation.
	if (delta == 0.) {
		return;
	}

    /* Resolve collisions */ {
        m_scene->collisions.clear();
        m_scene->entities.visit(CollisionReset());
        resolveAllCollisions();
    }

    /* Position update */ {
        ObjectCompute computeVisitor(delta);
        m_scene->entities.visit(computeVisitor);
        updateMainPosition();
    }

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
    constexpr auto horVel = 0.05f;
    constexpr auto vertVel = 0.01f;

    Entity::Vector<int, float, bool> vect0{};

    if (m_inputState->left.unsafeGet().state) {
        std::cout << "left\n";
        m_scene->entities.at<Entity::PhysicsForces>(0).thrusts.push_back(Entity::Thrust{.vector = {horVel, 0.f, 0.f}});
    }
    if (m_inputState->right.unsafeGet().state) {
        std::cout << "right\n";
        m_scene->entities.at<Entity::PhysicsForces>(0).thrusts.push_back(Entity::Thrust{.vector = {-horVel, 0.f, 0.f}});
    }
    if (m_inputState->down.unsafeGet().state) {
        std::cout << "down\n";
        m_scene->entities.at<Entity::PhysicsCartesianState>(0).velocity.x -= vertVel;
    }
    if (m_inputState->up.unsafeGet().state && !m_inputState->up.unsafeGet().hold) {
        std::cout << "up\n";
        m_scene->entities.at<Entity::PhysicsCartesianState>(0).velocity.x += vertVel;
    }
}
}
