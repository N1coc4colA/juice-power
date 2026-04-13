#ifndef JP_ENITY_COMPONENTS_H
#define JP_ENITY_COMPONENTS_H

#include <cstdint>

#include "src/defines.h"
#include "src/graphics/alignment.h"
#include "src/keywords.h"

namespace Entity {

struct _packed AnimationState
{
    /// @brief Id (offset) of the associated 6 vertices.
    GPU_EXPOSED(uint32_t, verticesId) = 0;
    /// @brief Currently used animation.
    GPU_EXPOSED(uint32_t, animationId) = 0;
    /// @brief Current animation's elapsed time.
    GPU_EXPOSED(float, animationTime) = 0.f;
};

struct _packed ObjectState
{
    /** @brief The object's position within the scene.
     *  The W value of the position should always be 1.f.
     *  @note This is glm:vec4 instead of glm::vec3 for alignment purposes, truth is that it must always be seen as a glm::vec3.
     **/
    GPU_EXPOSED(glm::vec4, position) {};
    /// @brief The object's transformation (size, pos...)
    GPU_EXPOSED(glm::mat4, transform) {};
};

struct _packed ObjectInformation
{
    /// @brief Object unique identifier.
    GPU_EXPOSED(uint32_t, objId) = std::numeric_limits<uint32_t>::max();
};

/**
 * @brief Axis-aligned bounding box used for broad-phase overlap checks.
 */
struct AABB
{
    /// @brief Minimum corner.
    glm::vec2 min{};
    /// @brief Maximum corner.
    glm::vec2 max{};

    /// @brief Returns true when two AABBs overlap.
    _nodiscard constexpr auto intersects(const AABB &other) const
    {
        if (max.x < other.min.x || min.x > other.max.x) {
            return false;
        }
        if (max.y < other.min.y || min.y > other.max.y) {
            return false;
        }

        // No separating axis found, therefor there is at least one overlapping axis
        return true;
    }
};

/**
 * @brief Aggregated linear and angular force result.
 */
struct Forces
{
    /// @brief Resulting linear force vector.
    glm::vec2 forces{};
    /// @brief Resulting angular velocity change.
    double angularVelocity = 0.;
};

/**
 * @brief Friction Information class
 * Represents which edges of an entity are affected by which amount of friction.
 * An empty @variable surfaces means all surfaces.
 */
struct Friction
{
    /// @brief Amount of friction on the surface(s).
    float friction = 0.f;
    /// @brief Surface(s) affected by the friction.
    /// Empty means all surfaces.
    std::vector<int> surfaces{};
};

/**
 * @brief Force applied at a specific point to create torque.
 */
struct Thrust
{
    /// @brief Force vector in world space.
    glm::vec3 vector = {};
    /// @brief Application point in world space.
    glm::vec3 point = {};
};

struct PhysicsBounds
{
    /// @brief Vectors representing the bounds of the entity.
    std::vector<glm::vec2> borders{};
    /// @brief Vectors corresponding to the normals associated to each vector in @variable borders.
    std::vector<glm::vec2> normals{};
};

struct PhysicsSetup
{
    /// @brief Orientation angle in radians.
    float angle = 0.f;
    /// @brief Coefficient of restitution.
    float elasticity = 0.f;
    /// @brief Mass of the entity
    float mass = 8.f;
    /// @brief Base friction value before transient updates.
    float baseFriction{};
    /// @brief Tells if other objects have an impact on the entity.
    /// @value true means the object will not be affected by other entity interactions.
    bool canCollide = true;
    /// @brief Tells if the object is affected by gravity or not.
    bool isNotFixed = true;
};

struct PhysicsConstraints
{
    /// @brief Frictions associated to this entity.
    float friction{};

    /// @brief Moment of Inertia.
    float MoI = 1.f;
};

struct PhysicsAngularState
{
    /// @brief Temporary angular velocity adjustment from collision solving.
    float temporaryAngularVelocities = 0.f;
    /// @brief Angular velocity in radians per second.
    float angularVelocity = 0.f;
};

struct PhysicsCartesianState
{
    /// @brief Current world position.
    glm::vec2 position{};
    /// @brief Current linear velocity.
    glm::vec2 velocity{};
    /// @brief Current linear acceleration.
    glm::vec2 acceleration{};
    /// @brief Temporary velocity adjustments from collision solving.
    glm::vec2 temporaryVelocities{};
};

struct PhysicsForces
{
    /// @brief Active thrust forces to apply this frame.
    std::vector<Thrust> thrusts{};
    /// @brief Active torques to apply this frame.
    std::vector<float> torques{};
};

struct PhysicsObjectState
{
    /// @brief Unique identifier of the object.
    uint32_t id = -1;
    /// @brief Indicates whether this entity has collision geometry.
    bool hasCollision = false;
};

/**
 * @brief Min/max scalar projection interval along a test axis.
 */
struct Projection
{
    /// @brief Minimum projection value.
    float minProj = 0.f;
    /// @brief Maximum projection value.
    float maxProj = 0.f;
    /// @brief Vertex index producing minimum projection.
    size_t minIndex = -1;
    /// @brief Vertex index producing maximum projection.
    size_t maxIndex = -1;
};

/**
 * @brief Contact information returned by narrow-phase collision tests.
 */
struct CollisionInfo
{
    /// @brief The collision's normal
    glm::vec2 normal{};
    /// @brief Contact point of the collision
    glm::vec2 point{};
    /// @brief Penetration depth of the collision
    float depth = 0.f;
};

using PhysicsEntity
    = TypesSet<PhysicsSetup, PhysicsObjectState, PhysicsBounds, AABB, PhysicsConstraints, PhysicsForces, PhysicsCartesianState, PhysicsAngularState>;
using GraphicsEntity = TypesSet<ObjectInformation, ObjectState, AnimationState>;

} // namespace Entity

#endif // JP_ENTITY_COMPONENTS_H
