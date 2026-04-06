#ifndef PHYSICS_DEFINES_H
#define PHYSICS_DEFINES_H

#include "src/config.h"

namespace Physics
{

static constexpr auto timestep() -> float
{
    return 1.f / Config::simTick / Config::simMultiplier * Config::simSpeed;
}


}


#endif // PHYSICS_DEFINES_H
