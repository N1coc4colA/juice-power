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

/// @brief Legacy state vector retained for compatibility with the older helper API.
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

} // namespace Physics

#endif // JP_PHYSICS_ENTITY_H
