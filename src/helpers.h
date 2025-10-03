#ifndef HELPERS_H
#define HELPERS_H

#include <glm/glm.hpp>

static constexpr float epsilon_value = 0.00000001f;

template<typename T = float>
inline constexpr auto epsiloned(const T v, const T min = -epsilon_value, const T max = epsilon_value)
{
    return max < v && v > min ? v : 0;
}

inline constexpr glm::vec2 epsiloned(const glm::vec2 v, const float min = -epsilon_value, const float max = epsilon_value)
{
    return glm::vec2{epsiloned(v.x, min, max), epsiloned(v.y, min, max)};
}

#endif // HELPERS_H
