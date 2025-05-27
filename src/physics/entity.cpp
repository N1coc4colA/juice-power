#include "entity.h"

#include <boost/numeric/odeint.hpp>

#include "../utils.h"


constexpr double GRAVITY = 0.005;
constexpr double FRICTION = 0.01;


namespace Physics
{

void Entity::KingKutta(const state_type &y, state_type &dydt, const double gravity, const double kx, const double ky, const double kt) const
{
	auto vx = y[0];
	auto vy = y[1];
	const auto &theta = y[2];

	if (isNotFixed) {
		vx = (static_cast<double>(utils::accumulate<utils::access<&Thrust::vector, &glm::vec3::x>>(thrusts, 0.f)) - kx*vx) / static_cast<double>(mass);
		// Because for us, Y is in the opposite direction, we have to invert the operation for the Weight & frictions.
		vy = (static_cast<double>(utils::accumulate<utils::access<&Thrust::vector, &glm::vec3::y>>(thrusts, 0.f)) - static_cast<double>(mass)*gravity - ky*vy) / static_cast<double>(mass);
	}

	dydt[0] = vx;
	dydt[1] = vy;

	// dtheta/dt
	// [NOTE] We currently ignore the effect of vert & horiz friction on net torque.
	dydt[2] = (
				static_cast<double>(utils::accumulate<utils::access<&Thrust::point>, utils::access<&Thrust::vector>, &utils::cross>(thrusts, 0.f))
				+ static_cast<double>(utils::accumulate(torques, 0.f))
				- kt*theta
			   ) / static_cast<double>(MoI);
}

inline constexpr glm::vec2 glm_vec2_addition(const glm::vec2 &a, const glm::vec2 &b)
{
	return a + b;
}

Forces Entity::resultOfForces(const double timeStep) const
{
	// [vx, vy, theta]
	state_type y {
        static_cast<double>(velocity.x + temporaryVelocities.x),
	    static_cast<double>(velocity.y + temporaryVelocities.y),
	    static_cast<double>(angularVelocity + temporaryAngularVelocities)
	};

	// Use runge_kutta4 stepper with constant time step, we may have to use a variable one in the future.
	boost::numeric::odeint::integrate_const(
		boost::numeric::odeint::runge_kutta4<state_type>(),
		// The system function
		[this](const state_type &y, state_type &dydt, const double t) {
			UNUSED(t);
			return KingKutta(y, dydt, GRAVITY, FRICTION, FRICTION, FRICTION);
		},
		y,              // Initial state
		0.0,            // Start time
		timeStep,       // End time
		timeStep/10.0   // Initial step size (smaller steps for accuracy)
	);

	return Forces {
		.forces = {y[0], y[1]},
		.angularVelocity = y[2],
	};
}

void Entity::compute(const double timeDelta)
{
	const Forces f = resultOfForces(timeDelta);
	angularVelocity = static_cast<float>(f.angularVelocity);

	// F = m*a, which means that a = F/m
	acceleration = f.forces;// / mass;
	velocity = nextVelocity(static_cast<float>(timeDelta));
	position = nextPosition(static_cast<float>(timeDelta));
}

void Entity::cleanup()
{
	temporaryVelocities = {};
	temporaryAngularVelocities = 0.f;
}

}
