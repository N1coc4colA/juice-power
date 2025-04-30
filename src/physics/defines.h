#ifndef PHYSICS_DEFINES_H
#define PHYSICS_DEFINES_H


namespace Physics
{

/// @brief Simulation runs per rendering tick.
static constexpr int sim_multiplier = 2;
/// @brief Simulation's speed, 1.0 = 100% speed.
static constexpr float sim_speed = 0.5;
/// @brief Simulation tick.
static constexpr int sim_tick = 60;

static constexpr float timestep()
{
	return 1.f / sim_tick / sim_multiplier * sim_speed;
}


}


#endif // PHYSICS_DEFINES_H
