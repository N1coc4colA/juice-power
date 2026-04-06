#ifndef CONFIG_H
#define CONFIG_H

namespace Config {
/// @brief Maximum number of objects that can reside in a scene.
constexpr int maximumObjectCount = 2048;
/// @brief Maximum number of objects that can move in a scene.
constexpr int maxMovingObjectCount = 20;
/// @brief Maximum number of non-moving objects in a scene.
constexpr int maxStaticObjectCount = maximumObjectCount - maxMovingObjectCount;
/// @brief Maximum number of resources that can be described when loading maps & such.
constexpr int maximumResourceTypeCount = 1024;
/// @brief Maximum height & width that an image can have.
constexpr int maximumImageSize = 2090;
constexpr unsigned long maxObjectsPerFrame = 200;
constexpr float eps2 = 1e-8f;
constexpr float potracePointError = 1e-12;
/// @brief Simulation runs per rendering tick.
static constexpr int simMultiplier = 2;
/// @brief Simulation's speed, 1.0 = 100% speed.
static constexpr float simSpeed = 0.5;
/// @brief Simulation tick.
static constexpr int simTick = 60;
/// @brief Numerical epsilon used in collision/math comparisons.
static constexpr float physicsEpsilon = 0.00001f;

} // namespace Config

#endif // CONFIG_H
