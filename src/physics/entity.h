#ifndef PHYSICS_ENTITY_H
#define PHYSICS_ENTITY_H

#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>

#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

#include "src/keywords.h"
#include "src/utils.h"

namespace Physics
{

/**
 * @brief Axis-aligned bounding box used for broad-phase overlap checks.
 */
struct AABB
{
	/// @brief Minimum corner.
	glm::vec2 min {};
	/// @brief Maximum corner.
	glm::vec2 max {};

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
	glm::vec2 normal {};
	/// @brief Contact point of the collision
	glm::vec2 point {};
	/// @brief Penetration depth of the collision
	float depth = 0.f;
};

/**
 * @brief Aggregated linear and angular force result.
 */
struct Forces
{
	/// @brief Resulting linear force vector.
	glm::vec2 forces {};
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
	std::vector<int> surfaces {};
};

/**
 * @brief Force applied at a specific point to create torque.
 */
struct Thrust
{
	/// @brief Force vector in world space.
	glm::vec3 vector;
	/// @brief Application point in world space.
	glm::vec3 point;
};


/**
 * @brief The Entity class
 */
class Entity
{
public:
    /// @brief State vector used by Runge-Kutta integration.
    using state_type = std::array<double, 3>;

    /// @brief Stable unique entity identifier.
    size_t id = std::numeric_limits<size_t>::max();

    /// @brief Indicates whether this entity has collision geometry.
    bool has_collision = false;

    /// @brief Broad-phase bounding box.
    AABB boundingBox {};

	/// @brief Vectors representing the bounds of the entity.
	std::vector<glm::vec2> borders {};
	/// @brief Vectors corresponding to the normals associated to each vector in @variable borders.
	std::vector<glm::vec2> normals {};
	/// @brief Frictions associated to this entity.
    float friction{};
    /// @brief Base friction value before transient updates.
    float baseFriction{};

    /// @brief Active thrust forces to apply this frame.
    std::vector<Thrust> thrusts {};
	/// @brief Active torques to apply this frame.
	std::vector<float> torques {};

	/// @brief Current world position.
	glm::vec2 position {};
	/// @brief Current linear velocity.
	glm::vec2 velocity {};
	/// @brief Current linear acceleration.
	glm::vec2 acceleration {};

	/// @brief Temporary velocity adjustments from collision solving.
	glm::vec2 temporaryVelocities {};
	/// @brief Temporary angular velocity adjustment from collision solving.
	float temporaryAngularVelocities = 0.f;

	/// @brief Mass of the entity
	float mass = 8.f;
	/// @brief Orientation angle in radians.
	float angle = 0.f;
	/// @brief Angular velocity in radians per second.
	float angularVelocity = 0.f;
    /// @brief Coefficient of restitution.
    float elasticity = 0.f;
    /// @brief Moment of Inertia.
    float MoI = 1.f;

    /// @brief Tells if other objects have an impact on the entity.
	/// @value true means the object will not be affected by other entity interactions.
    bool canCollide = true;
    /// @brief Tells if the object is affected by gravity or not.
	bool isNotFixed = true;

    /// @brief Enables verbose collision logs through environment variable.
    const bool debug = getenv("JUICE_DEBUG_COLLISIONS") != nullptr;

    /// @brief Predicts next position for a given delta.
    _nodiscard constexpr auto nextPosition(const float timeDelta) const noexcept { return position + velocity * timeDelta; }

    /// @brief Predicts next velocity for a given delta.
    _nodiscard constexpr auto nextVelocity(const float timeDelta) const noexcept { return velocity + acceleration * timeDelta; }

    // Used for integration of forces. Here it's not King Kunta but King Kutta !!
    /// @brief Computes RK derivative for translational/rotational state.
    void KingKutta(
        const Entity::state_type &y, Entity::state_type &dydt, const double gravity, const double kx, const double ky, const double kt) const;
    /// @brief Aggregates all active forces over the given timestep.
    _nodiscard auto resultOfForces(const double timeStep) const -> Forces;
    /// @brief Advances simulation state to next frame.
    void compute(const double timeDelta);

    /// @brief Returns geometric center from position and bounding box extents.
    _nodiscard inline constexpr auto center() const noexcept { return position + (boundingBox.min + boundingBox.max) * 0.5f; }

    /// @brief Projects polygon borders onto an axis and returns min/max interval.
    _nodiscard constexpr auto getMinMax(const std::span<const glm::vec2> &borders, const glm::vec2 &axis) const noexcept
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

        return Projection{
            .minProj = minProj,
            .maxProj = maxProj,
            .minIndex = minProjIndex,
            .maxIndex = maxProjIndex,
        };
    }

    /// @brief SAT collision test against another entity.
    _nodiscard auto collides(const Entity &other, CollisionInfo &info) const noexcept
    {
        // t: this, o: other, b: borders, n: normals
        const auto tb = std::span<const glm::vec2>(borders.data(), borders.size());
        const auto tn = std::span<const glm::vec2>(normals.data(), normals.size());
        const auto ob = std::span<const glm::vec2>(other.borders.data(), other.borders.size());
        const auto on = std::span<const glm::vec2>(other.normals.data(), other.normals.size());

        float minOverlap = std::numeric_limits<float>::max();
        glm::vec2 smallestAxis{};

        if (debug) {
            std::cout << "--- COLLISION CHECK: this id=" << id << " other id=" << other.id << "\n";
            std::cout << "this pos=" << position.x << "," << position.y << " center=" << center().x << "," << center().y << "\n";
            std::cout << "other pos=" << other.position.x << "," << other.position.y << " center=" << other.center().x << "," << other.center().y << "\n";
            std::cout << "this borders count=" << borders.size() << " normals count=" << normals.size() << "\n";
            std::cout << "other borders count=" << other.borders.size() << " normals count=" << other.normals.size() << "\n";
        }

        {
			for (const auto &normal : tn) {
				const auto firstIntersect = getMinMax(tb, normal);
				const auto secondIntersect = other.getMinMax(ob, normal);

				const bool isSeparated = firstIntersect.maxProj < secondIntersect.minProj || secondIntersect.maxProj < firstIntersect.minProj;
				if (isSeparated) {
					if (debug) {
						std::cout << "Separated on this's axis: (" << normal.x << "," << normal.y << ")\n";
						std::cout << "  this proj: [" << firstIntersect.minProj << "," << firstIntersect.maxProj << "] other proj: [" << secondIntersect.minProj << "," << secondIntersect.maxProj << "]\n";
					}

                    return false;
				}

				const float overlap = std::min(firstIntersect.maxProj - secondIntersect.minProj, secondIntersect.maxProj - firstIntersect.minProj);
				if (debug) {
					std::cout << "Axis (this): (" << normal.x << "," << normal.y << ") proj this=[" << firstIntersect.minProj << "," << firstIntersect.maxProj << "] other=[" << secondIntersect.minProj << "," << secondIntersect.maxProj << "] overlap=" << overlap << "\n";
				}
				if (overlap < minOverlap) {
					minOverlap = overlap;
					smallestAxis = normal;
				}
			}
		}

		{
			for (const auto &normal : on) {
				const auto firstIntersect = getMinMax(tb, normal);
				const auto secondIntersect = other.getMinMax(ob, normal);

				const bool isSeparated = firstIntersect.maxProj < secondIntersect.minProj || secondIntersect.maxProj < firstIntersect.minProj;
				if (isSeparated) {
					if (debug) {
						std::cout << "Separated on other's axis: (" << normal.x << "," << normal.y << ")\n";
						std::cout << "  this proj: [" << firstIntersect.minProj << "," << firstIntersect.maxProj << "] other proj: [" << secondIntersect.minProj << "," << secondIntersect.maxProj << "]\n";
					}

                    return false;
				}

				const float overlap = std::min(firstIntersect.maxProj - secondIntersect.minProj, secondIntersect.maxProj - firstIntersect.minProj);
				if (debug) {
					std::cout << "Axis (other): (" << normal.x << "," << normal.y << ") proj this=[" << firstIntersect.minProj << "," << firstIntersect.maxProj << "] other=[" << secondIntersect.minProj << "," << secondIntersect.maxProj << "] overlap=" << overlap << "\n";
				}
				if (overlap < minOverlap) {
					minOverlap = overlap;
					smallestAxis = normal;
				}
			}
		}

		// Ensure the normal points from this entity to the other entity.
		// Use (other.center - center) so a negative dot indicates the axis
		// points away from the other entity and must be inverted.
		const auto centerDelta = other.center() - center();
		if (glm::dot(centerDelta, smallestAxis) < 0.f) {
			smallestAxis = -smallestAxis;
		}

        info.normal = glm::normalize(smallestAxis);
        info.depth = minOverlap;

        return true;
    }

    /// @brief Performs cleanups for post-position update.
    void cleanup();
};


}


#endif // PHYSICS_ENTITY_H
