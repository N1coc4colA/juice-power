#ifndef JP_PHYSICS_ENTITY_H
#define JP_PHYSICS_ENTITY_H

#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>

#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

#include "src/entity/components.h"
#include "src/keywords.h"

namespace Physics
{

/// @brief State vector used by Runge-Kutta integration.
using state_type = std::array<double, 3>;
using ComputeParameters = ReferencesSet<Entity::PhysicsCartesianState,
                                        Entity::PhysicsAngularState,
                                        const Entity::PhysicsSetup,
                                        Entity::PhysicsForces,
                                        const Entity::PhysicsConstraints>;

/// @brief Predicts next position for a given delta.
_nodiscard constexpr auto nextPosition(const Entity::PhysicsCartesianState &state, const float timeDelta) noexcept
{
    return state.position + state.velocity * timeDelta;
}

/// @brief Predicts next velocity for a given delta.
_nodiscard constexpr auto nextVelocity(const Entity::PhysicsCartesianState &state, const float timeDelta) noexcept
{
    return state.velocity + state.acceleration * timeDelta;
}

/// @brief Returns geometric center from position and bounding box extents.
_nodiscard constexpr auto center(const Entity::PhysicsCartesianState &state, const Entity::AABB &boundingBox) noexcept
{
    return state.position + (boundingBox.min + boundingBox.max) * 0.5f;
}

/// @brief Projects polygon borders onto an axis and returns min/max interval.
_nodiscard constexpr auto getMinMax(const glm::vec2 &position, const std::span<const glm::vec2> &borders, const glm::vec2 &axis) noexcept
{
    float minProj = glm::dot(position + borders[0], axis);
    float maxProj = minProj;
    size_t minProjIndex = 0;
    size_t maxProjIndex = 0;

    const auto size = borders.size();
    for (size_t i = 1; i < size; i++) {
        const float proj = glm::dot(position + borders[i], axis);
        if (minProj > proj) {
            minProj = proj;
            minProjIndex = i;
        }
        if (maxProj < proj) {
            maxProj = proj;
            maxProjIndex = i;
        }
    }

    return ::Entity::Projection{
        .minProj = minProj,
        .maxProj = maxProj,
        .minIndex = minProjIndex,
        .maxIndex = maxProjIndex,
    };
}

/// @brief Aggregates all active forces over the given timestep.
_nodiscard auto resultOfForces(glm::vec2 velocity,
                               float angularVelocity,
                               float temporaryAngularVelocities,
                               float friction,
                               double mass,
                               bool isNotFixed,
                               const std::vector<Entity::Thrust> &thrusts,
                               double timeStep) -> Entity::Forces;

/// @brief Advances simulation state to next frame.
void compute(double timeDelta, ComputeParameters params);

class ComputeState
{
public:
    /// @brief Enables verbose collision logs through environment variable.
    const bool debug = getenv("JUICE_DEBUG_COLLISIONS") != nullptr;

    using DebugParameters
        = ReferencesSet<const Entity::PhysicsObjectState, const Entity::PhysicsCartesianState, const Entity::AABB, const Entity::PhysicsBounds>;
    using CollisionParameters = ReferencesSet<const Entity::PhysicsSetup,
                                              const Entity::PhysicsObjectState,
                                              const Entity::PhysicsBounds,
                                              const Entity::AABB,
                                              const Entity::PhysicsConstraints,
                                              const Entity::PhysicsForces,
                                              const Entity::PhysicsCartesianState,
                                              const Entity::PhysicsAngularState>;

    static void printDebugData(DebugParameters a, DebugParameters b)
    {
        auto &[a_objState, a_cState, a_boundingBox, a_bounds] = a;
        auto &[b_objState, b_cState, b_boundingBox, b_bounds] = b;

        const auto aCenter = center(a_cState, a_boundingBox);
        const auto bCenter = center(b_cState, b_boundingBox);

        std::cout << "--- COLLISION CHECK: this id=" << a_objState.id << " other id=" << b_objState.id << "\n";
        std::cout << "this pos=" << a_cState.position.x << "," << a_cState.position.y << " center=" << aCenter.x << "," << aCenter.y << "\n";
        std::cout << "other pos=" << b_cState.position.x << "," << b_cState.position.y << " center=" << bCenter.x << "," << bCenter.y << "\n";
        std::cout << "this borders count=" << a_bounds.borders.size() << " normals count=" << a_bounds.normals.size() << "\n";
        std::cout << "other borders count=" << b_bounds.borders.size() << " normals count=" << b_bounds.normals.size() << "\n";
    }

    /// @brief SAT collision test against another entity.
    _nodiscard auto collides(CollisionParameters a, CollisionParameters b, ::Entity::CollisionInfo &info) const noexcept
    {
        auto &[a_setup, a_objState, a_bounds, a_bBox, a_constraints, a_forces, a_cState, a_aState] = a;
        auto &[b_setup, b_objState, b_bounds, b_bBox, b_constraints, b_forces, b_cState, b_aState] = b;

        // t: this, o: other, b: borders, n: normals
        const auto tb = std::span(a_bounds.borders.data(), a_bounds.borders.size());
        const auto tn = std::span(a_bounds.normals.data(), a_bounds.normals.size());
        const auto ob = std::span(b_bounds.borders.data(), b_bounds.borders.size());
        const auto on = std::span(b_bounds.normals.data(), b_bounds.normals.size());

        float minOverlap = std::numeric_limits<float>::max();
        glm::vec2 smallestAxis{};

        if (debug) {
            printDebugData({a_objState, a_cState, a_bBox, a_bounds}, {b_objState, b_cState, b_bBox, b_bounds});
        }

        for (const auto &normal : tn) {
            const auto firstIntersect = getMinMax(a_cState.position, tb, normal);
            const auto secondIntersect = getMinMax(b_cState.position, ob, normal);

            // Check if it is separated.
            if (firstIntersect.maxProj < secondIntersect.minProj || secondIntersect.maxProj < firstIntersect.minProj) {
                if (debug) {
                    std::cout << "Separated on this's axis: (" << normal.x << "," << normal.y << ")\n";
                    std::cout << "  this proj: [" << firstIntersect.minProj << "," << firstIntersect.maxProj << "] other proj: ["
                              << secondIntersect.minProj << "," << secondIntersect.maxProj << "]\n";
                }

                return false;
            }

            const float overlap = std::min(firstIntersect.maxProj - secondIntersect.minProj, secondIntersect.maxProj - firstIntersect.minProj);
            if (debug) {
                std::cout << "Axis (this): (" << normal.x << "," << normal.y << ") proj this=[" << firstIntersect.minProj << ","
                          << firstIntersect.maxProj << "] other=[" << secondIntersect.minProj << "," << secondIntersect.maxProj
                          << "] overlap=" << overlap << "\n";
            }
            if (overlap < minOverlap) {
                minOverlap = overlap;
                smallestAxis = normal;
            }
        }

        for (const auto &normal : on) {
            const auto firstIntersect = getMinMax(a_cState.position, tb, normal);
            const auto secondIntersect = getMinMax(b_cState.position, ob, normal);

            // Check if it is separated
            if (firstIntersect.maxProj < secondIntersect.minProj || secondIntersect.maxProj < firstIntersect.minProj) {
                if (debug) {
                    std::cout << "Separated on other's axis: (" << normal.x << "," << normal.y << ")\n";
                    std::cout << "  this proj: [" << firstIntersect.minProj << "," << firstIntersect.maxProj << "] other proj: ["
                              << secondIntersect.minProj << "," << secondIntersect.maxProj << "]\n";
                }

                return false;
            }

            const float overlap = std::min(firstIntersect.maxProj - secondIntersect.minProj, secondIntersect.maxProj - firstIntersect.minProj);
            if (debug) {
                std::cout << "Axis (other): (" << normal.x << "," << normal.y << ") proj this=[" << firstIntersect.minProj << ","
                          << firstIntersect.maxProj << "] other=[" << secondIntersect.minProj << "," << secondIntersect.maxProj
                          << "] overlap=" << overlap << "\n";
            }
            if (overlap < minOverlap) {
                minOverlap = overlap;
                smallestAxis = normal;
            }
        }

        // Ensure the normal points from this entity to the other entity.
        // Use (other.center - center) so a negative dot indicates the axis
        // points away from the other entity and must be inverted.
        const auto aCenter = center(a_cState, a_bBox);
        const auto bCenter = center(b_cState, b_bBox);
        if (const auto centerDelta = bCenter - aCenter; glm::dot(centerDelta, smallestAxis) < 0.f) {
            smallestAxis = -smallestAxis;
        }

        info.normal = glm::normalize(smallestAxis);
        info.depth = minOverlap;

        return true;
    }
};

} // namespace Physics

#endif // JP_PHYSICS_ENTITY_H
