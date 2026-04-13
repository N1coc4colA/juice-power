#include "src/physics/entity.h"

#include <boost/numeric/odeint.hpp>

#include "src/defines.h"
#include "src/utils.h"


constexpr double GRAVITY = -0.05;

namespace Physics
{

/**
 * @brief Used for integration of forces. Here it's not King Kunta but King Kutta !!
 * Runge-Kutta is an integration method.
 **/
void KingKutta(const state_type &y,
               state_type &dydt,
               const double gravity,
               const double mass,
               const bool isNotFixed,
               const std::vector<Entity::Thrust> &thrusts,
               const double kx,
               const double ky,
               const double kt)
{
	unused(kt);

	auto vx = y[0];
	auto vy = y[1];
    //const auto &theta = y[2];

    if (isNotFixed) {
        vx = (static_cast<double>(utils::accumulate<utils::Access<&Entity::Thrust::vector, &glm::vec3::x>>(thrusts, 0.f)) - kx * vx)
             / static_cast<double>(mass);
        // Because for us, Y is in the opposite direction, we have to invert the operation for the Weight & frictions.
        vy = (static_cast<double>(utils::accumulate<utils::Access<&Entity::Thrust::vector, &glm::vec3::y>>(thrusts, 0.f))
              + static_cast<double>(mass) * gravity - ky * vy)
             / static_cast<double>(mass);
    }

    dydt[0] = vx;
    dydt[1] = vy;
    dydt[2] = y[2];

    // dtheta/dt
    // [NOTE] We currently ignore the effect of vert & horiz friction on net torque.
    /*dydt[2] = (static_cast<double>(utils::accumulate<utils::access<&Thrust::point>, utils::access<&Thrust::vector>, &utils::cross>(thrusts, 0.f))
               + static_cast<double>(utils::accumulate(torques, 0.f)) - kt * theta)
              / static_cast<double>(MoI);*/
}

auto resultOfForces(const glm::vec2 velocity,
                    const float angularVelocity,
                    const float temporaryAngularVelocities,
                    const float friction,
                    const double mass,
                    const bool isNotFixed,
                    const std::vector<Entity::Thrust> &thrusts,
                    const double timeStep) -> Entity::Forces
{
	// [vx, vy, theta]
    state_type y{static_cast<double>(velocity.x), static_cast<double>(velocity.y), static_cast<double>(angularVelocity + temporaryAngularVelocities)};

    // Use runge_kutta4 stepper with constant time step, we may have to use a variable one in the future.
    boost::numeric::odeint::integrate_const(
        boost::numeric::odeint::runge_kutta4<state_type>(),
        // The system function
        [friction, mass, isNotFixed, &thrusts](const state_type &yValue, state_type &dydtValue, const double t) -> auto {
            unused(t);
            return KingKutta(yValue, dydtValue, GRAVITY, mass, isNotFixed, thrusts, friction, friction, friction);
        },
        y,              // Initial state
        0.0,            // Start time
        timeStep,       // End time
        timeStep / 10.0 // Initial step size (smaller steps for accuracy)
    );

    return Entity::Forces{
        .forces = {y[0], y[1]},
        .angularVelocity = y[2],
    };
}

void compute(const double timeDelta, ComputeParameters params)
{
    auto &[cState, aState, pSetup, pForces, pConstraints] = params;

    if (pSetup.isNotFixed) {
        const auto [forces, fAngularVelocity] = resultOfForces(cState.velocity,
                                                               aState.angularVelocity,
                                                               aState.temporaryAngularVelocities,
                                                               pConstraints.friction,
                                                               pSetup.mass,
                                                               pSetup.isNotFixed,
                                                               pForces.thrusts,
                                                               timeDelta);
        aState.angularVelocity = static_cast<float>(fAngularVelocity);

        // F = m*a, which means that a = F/m
        cState.acceleration = forces; // / mass;
        cState.velocity = nextVelocity(cState, static_cast<float>(timeDelta));
        cState.position = nextPosition(cState, static_cast<float>(timeDelta));

        pForces.thrusts.clear();
    }
}
}
