#ifndef PHYSICS_ENTITY_H
#define PHYSICS_ENTITY_H

#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>

#include <span>
#include <vector>


namespace Physics
{

struct AABB
{
	glm::vec2 min {};
	glm::vec2 max {};

	constexpr bool intersects(const AABB &other) const
	{
		if(max.x < other.min.x || min.x > other.max.x) {
			return false;
		}
		if(max.y < other.min.y || min.y > other.max.y) {
			return false;
		}

		// No separating axis found, therefor there is at least one overlapping axis
		return true;
	}
};

struct Projection
{
	float minProj = 0.f;
	float maxProj = 0.f;
	size_t minIndex = -1;
	size_t maxIndex = -1;
};

struct Forces
{
	glm::vec2 forces {};
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

struct Thrust
{
	glm::vec3 vector;
	glm::vec3 point;
};


/**
 * @brief The Entity class
 */
class Entity
{
public:
	typedef std::array<double, 3> state_type;

	AABB boundingBox {};

	/// @brief Vectors representing the bounds of the entity.
	std::vector<glm::vec2> borders {};
	/// @brief Vectors corresponding to the normals associated to each vector in @variable borders.
	std::vector<glm::vec2> normals {};
	/// @brief Frictions associated to this entity.
	std::vector<Friction> frictions {};

	std::vector<Thrust> thrusts {};
	std::vector<float> torques{};

	glm::vec2 position {};
	glm::vec2 velocity {};
	glm::vec2 acceleration {};

	/// @brief Mass of the entity
	float mass = 8.f;
	float angle = 0.f;
	float angularVelocity = 0.f;
	/// @brief Moment of Inertia.
	float MoI = 0.f;

	/// @brief Tells if other objects have an impact on the entity.
	/// @value true means the object will not be affected by other entity's interactions.
	bool canCollide = false;
	/// @brief Tells if the object is affected by gravity or not.
	bool hasGravity = false;
	bool isFixed = false;

	constexpr glm::vec2 nextPosition(const float timeDelta) const noexcept
	{
		return position + velocity*timeDelta;
	}

	constexpr glm::vec2 nextVelocity(const float timeDelta) const noexcept
	{
		return velocity + acceleration*timeDelta;
	}

	// Used for integration of forces. Here it's not King Kunta but King Kutta !!
	void KingKutta(const Entity::state_type &y, Entity::state_type &dydt, const double gravity, const double kx, const double ky, const double kt) const;
	Forces resultOfForces(const double timeStep) const;
	void compute(const double timeDelta);

	inline constexpr glm::vec2 center() const noexcept
	{
		return position + (boundingBox.max - boundingBox.min) / 2.f;
	}

	constexpr Projection getMinMax(const std::span<const glm::vec2> &borders, const glm::vec2 &axis) const noexcept
	{
		float minProj = glm::dot(borders[0], axis);
		float maxProj = minProj;
		size_t minProjIndex = 0;
		size_t maxProjIndex = 0;

		const auto size = borders.size();
		for (size_t i = 1; i < size; i++) {
			const float proj = glm::dot(borders[i], axis);
			if (minProj > proj) {
				minProj = proj;
				minProjIndex = i;
			}
			if (maxProj < proj) {
				maxProj = proj;
				maxProjIndex = i;
			}
		}

		return Projection {
			.minProj = minProj,
			.maxProj = maxProj,
			.minIndex = minProjIndex,
			.maxIndex = maxProjIndex,
		};
	}

	constexpr bool collides(const Entity &other, Projection &outProj) const noexcept
	{
		// t: this, o: other, b: borders, n: normals
		const auto tb = std::span<const glm::vec2>(borders.data(), borders.size());
		const auto tn = std::span<const glm::vec2>(normals.data(), normals.size());
		const auto ob = std::span<const glm::vec2>(other.borders.data(), other.borders.size());
		const auto on = std::span<const glm::vec2>(other.normals.data(), other.normals.size());

		bool isSeparated = false;
		{
			const auto size = normals.size();
			for (size_t i = 0; i < size; i++) {
				const auto firstIntersect = getMinMax(tb, tn[i]);
				const auto secondIntersect = getMinMax(ob, tn[i]);

				isSeparated = firstIntersect.maxProj < secondIntersect.minProj || secondIntersect.maxProj < firstIntersect.minProj;
				if (isSeparated) {
					return true;
				}
			}
		}

		if (!isSeparated) {
			const auto size = other.normals.size();
			for (size_t i = 0; i < size; i++) {
				const auto firstIntersect = getMinMax(tb, on[i]);
				const auto secondIntersect = getMinMax(ob, on[i]);

				isSeparated = firstIntersect.maxProj < secondIntersect.minProj || secondIntersect.maxProj < firstIntersect.minProj;
				if (isSeparated) {
					return true;
				}
			}
		}

		return false;
	}
};


}


#endif // PHYSICS_ENTITY_H
