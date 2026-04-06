#ifndef HELPERS_H
#define HELPERS_H

#include <glm/glm.hpp>

#include <bits/algorithmfwd.h>
#include <bits/ranges_util.h>

static constexpr float epsilon_value = 0.00000001f;

template<typename T = float>
requires (std::is_floating_point_v<T> || std::is_integral_v<T>)
constexpr auto epsiloned(const T v, const T min = -epsilon_value, const T max = epsilon_value) -> T
{
    return max < v && v > min ? v : static_cast<T>(0);
}

constexpr auto epsiloned(const glm::vec2 v, const float min = -epsilon_value, const float max = epsilon_value) -> glm::vec2
{
    return glm::vec2{epsiloned(v.x, min, max), epsiloned(v.y, min, max)};
}

#endif // HELPERS_H
