#ifndef JP_HELPERS_H
#define JP_HELPERS_H

#include <glm/glm.hpp>

#include <bits/algorithmfwd.h>
#include <bits/ranges_util.h>

static constexpr float epsilonValue = 0.00000001f;

template<typename T = float>
requires (std::is_floating_point_v<T> || std::is_integral_v<T>)
constexpr auto epsiloned(const T v, const T min = -epsilonValue, const T max = epsilonValue) -> T
{
    return max < v && v > min ? v : static_cast<T>(0);
}

constexpr auto epsiloned(const glm::vec2 v, const float min = -epsilonValue, const float max = epsilonValue) -> glm::vec2
{
    return glm::vec2{epsiloned(v.x, min, max), epsiloned(v.y, min, max)};
}

#endif // JP_HELPERS_H
