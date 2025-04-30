#include "entity.h"

#include <boost/numeric/odeint.hpp>

#include "../utils.h"


constexpr double GRAVITY = 0.005;
constexpr double FRICTION = 0.01f;


namespace Physics
{

void Entity::KingKutta(const state_type &y, state_type &dydt, const double t, const double gravity, const double kx, const double ky, const double kt) const
{
	auto vx = y[0];
	auto vy = y[1];
	const auto &theta = y[2];

	if (!isFixed) {
		vx = (utils::accumulate<utils::access<&Thrust::vector, &glm::vec3::x>>(thrusts, 0.f) - kx*vx) / mass;
		// Because for us, Y is in the opposite direction, we have to invert the operation for the Weight & frictions.
		vy = (utils::accumulate<utils::access<&Thrust::vector, &glm::vec3::y>>(thrusts, 0.f) + mass*gravity + ky*vy) / mass;
	}

	dydt[0] = vx;
	dydt[1] = vy;

	// dtheta/dt
	// [NOTE] We currently ignore the effect of vert & horiz friction on net torque.
	dydt[2] = (
				utils::accumulate<utils::access<&Thrust::point>, utils::access<&Thrust::vector>, &utils::cross>(thrusts, 0.f)
				+ utils::accumulate(torques, 0.f)
				- kt*theta
			  ) / MoI;
}

Forces Entity::resultOfForces(const double timeStep) const
{
	// [vx, vy, theta]
	state_type y {velocity.x, velocity.y, angularVelocity};

	// Use runge_kutta4 stepper with constant time step, we may have to use a variable one in the future.
	boost::numeric::odeint::integrate_const(
		boost::numeric::odeint::runge_kutta4<state_type>(),
		// The system function
		[this](const state_type &y, state_type &dydt, const double t) {
			return KingKutta(y, dydt, t, GRAVITY, FRICTION, FRICTION, FRICTION);
		},
		y,              // Initial state
		0.0,            // Start time
		timeStep,       // End time
		timeStep/10.0   // Initial step size (smaller steps for accuracy)
	);

	return Forces {
		.forces = {y[0], y[1]},
		.angularVelocity = static_cast<float>(y[2])
	};
}

void Entity::compute(const double timeDelta)
{
	const Forces f = resultOfForces(timeDelta);
	angularVelocity = f.angularVelocity;

	// F = m*a, which means that a = F/m
	acceleration = f.forces;// / mass;
	velocity = nextVelocity(timeDelta);
	position = nextPosition(timeDelta);
}


}
