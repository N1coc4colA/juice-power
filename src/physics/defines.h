#ifndef JP_PHYSICS_DEFINES_H
#define JP_PHYSICS_DEFINES_H

#include "src/config.h"

namespace Physics
{

static constexpr auto timestep() -> float
{
    return 1.f / Config::simTick / Config::simMultiplier * Config::simSpeed;
}


}

#endif // JP_PHYSICS_DEFINES_H
