#include "src/physics/entity.h"

#include "src/defines.h"

namespace Physics
{

auto resultOfForces(const glm::vec2 velocity,
                  const float angularVelocity,
                  const float temporaryAngularVelocities,
                  const float friction,
                  const double mass,
                  const bool isNotFixed,
                  const std::vector<Entity::Thrust> &thrusts,
                  const double timeStep) -> Entity::Forces
{
    unused(friction);
    unused(mass);
    unused(isNotFixed);
    unused(thrusts);
    unused(timeStep);

    return Entity::Forces{
        .forces = velocity,
        .angularVelocity = angularVelocity + temporaryAngularVelocities,
    };
}

void compute(const double timeDelta, ComputeParameters params)
{
    auto &[cState, aState, pSetup, pForces, pConstraints] = params;

    unused(timeDelta);
    unused(aState);
    unused(pSetup);
    unused(pForces);
    unused(pConstraints);

    // Box2D owns the actual simulation in Engine::compute(), so this compatibility
    // helper is intentionally inert to avoid reintroducing the legacy integration path.
    cState.acceleration = glm::vec2{};
}
}
