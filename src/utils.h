#ifndef UTILS_H
#define UTILS_H

#include <glm/fwd.hpp>
#include <glm/geometric.hpp>

#include <functional>
#include <iterator>
#include <numeric>
#include <ranges>
#include <type_traits>

namespace utils
{

/// @brief Returns 2D cross product z-component.
constexpr auto cross(const glm::vec2 &a, const glm::vec2 &b) noexcept -> float
{
    return a.x * b.y - b.x * a.y;
}

/// @brief Re-export of std::accumulate.
using std::accumulate;

/**
 * @brief Resolves nested member access path.
 */
template<auto Member, auto... Members>
struct Access
{
    /// @brief Type of the final accessed member.
    using type = Access<Members...>::type;

    /// @brief Returns nested member reference from an instance.
    template<typename I>
    static constexpr auto get(I &&instance) -> auto &&
    {
        return Access<Members...>::get(instance.*Member);
    }
};

/**
 * @brief Base case for one-level member access path.
 */
template<auto Member>
struct Access<Member>
{
	/// @brief Type of the accessed member pointer.
	using type = std::remove_reference_t<decltype(Member)>;

    /// @brief Returns direct member reference from an instance.
    template<typename I>
    static constexpr auto get(I &instance) -> auto &
    {
        return instance.*Member;
    }
};

/// @brief Trait indicating if a type is an Access path.
template<typename T>
struct IsAccess : std::false_type
{};

/// @brief Specialization marking Access paths.
template<auto... Args>
struct IsAccess<Access<Args...>> : std::true_type
{};


/// @brief Accumulates raw iterator values with an operation.
template<typename T, auto Operation = std::plus(), class InputIterator>
constexpr auto accumulate(InputIterator first, InputIterator last, T init) -> T
{
	if constexpr (is_callable<T, T>(Operation)) {
		for (; first != last; ++first) {
			init = Operation(std::move(init), *first);
		}
	} else {
		for (; first != last; ++first) {
			init = (*Operation)(std::move(init), *first);
		}
	}

	return init;
}

/// @brief Accumulates projected values from one member path.
template<typename Path, typename T, auto Operation = std::plus(), class InputIterator>
    requires IsAccess<Path>::value
constexpr auto accumulate(InputIterator first, InputIterator last, T init) -> T
{
	for (; first != last; ++first) {
		init = std::invoke(Operation, std::move(init), Path::get(*first));
	}

	return init;
}

/// @brief Accumulates values composed of two member paths on one range.
template<typename Path0, typename Path1, typename T, auto Operation = std::plus<>(), auto Composer = std::plus(), class InputIterator>
    requires IsAccess<Path0>::value && IsAccess<Path1>::value
constexpr auto accumulate(InputIterator first, InputIterator last, T init) -> T
{
	for (; first != last; ++first) {
		init = std::invoke(Operation, std::move(init), std::invoke(Composer, Path0::get(*first), Path1::get(*first)));
	}

	return init;
}

/// @brief Accumulates values composed of two ranges.
template<typename Path0, typename Path1, typename T, auto Operation = std::plus(), auto Composer = std::plus(), class InputIterator0, class InputIterator1>
    requires IsAccess<Path0>::value && IsAccess<Path1>::value
constexpr auto accumulate(InputIterator0 first0, InputIterator0 last0, InputIterator1 first1, InputIterator1 last1, T init) -> T
{
	for (; first0 != last0 && first1 != last1; ++first0, ++first1) {
		init = std::invoke(Composer, std::move(init), std::invoke(Operation, Path0::get(*first0), Path1::get(*first1)));
	}

	return init;
}

/// @brief Range overload delegating to iterator accumulate.
template<std::ranges::range Container, typename T, auto Operation = std::plus()>
constexpr auto accumulate(const Container &container, T init) -> T
{
	return accumulate<T, Operation>(
		std::begin(container), std::end(container),
		init
	);
}

/// @brief Range overload accumulating projected values by one path.
template<typename Path, std::ranges::range Container, typename T, auto Operation = std::plus()>
    requires IsAccess<Path>::value
constexpr auto accumulate(const Container &container, T init) -> T
{
	return accumulate<Path, T, Operation>(
		std::begin(container), std::end(container),
		init
	);
}

/// @brief Range overload accumulating composed values by two paths.
template<typename Path0, typename Path1, std::ranges::range Container, typename T, auto Operation = std::plus(), auto Composer = std::plus()>
    requires IsAccess<Path0>::value && IsAccess<Path1>::value
constexpr auto accumulate(const Container &container, T init) -> T
{
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container), std::end(container),
		init
	);
}

/// @brief Alternate template-parameter order for two-path range accumulate.
template<typename Path0, typename Path1, auto Composer = std::plus(), auto Operation = std::plus(), typename T, std::ranges::range Container>
    requires IsAccess<Path0>::value && IsAccess<Path1>::value
constexpr auto accumulate(const Container &container, T init) -> T
{
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container), std::end(container),
		init
	);
}

/// @brief Two-range overload accumulating composed projected values.
template<typename Path0,
         typename Path1,
         std::ranges::range Container0,
         std::ranges::range Container1,
         typename T,
         auto Composer = std::plus(),
         auto Operation = std::plus()>
    requires IsAccess<Path0>::value && IsAccess<Path1>::value
constexpr auto accumulate(const Container0 &container0, const Container1 &container1, T init) -> T
{
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container0), std::end(container0),
		std::begin(container1), std::end(container1),
		init
	);
}

/// @brief Alternate template-parameter order for two-range accumulate.
template<typename Path0,
         typename Path1,
         auto Composer = std::plus(),
         auto Operation = std::plus(),
         typename T,
         std::ranges::range Container0,
         std::ranges::range Container1>
    requires IsAccess<Path0>::value && IsAccess<Path1>::value
constexpr auto accumulate(const Container0 &container0, const Container1 &container1, T init) -> T
{
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container0), std::end(container0),
		std::begin(container1), std::end(container1),
		init
	);
}

} // namespace utils

#endif // UTILS_H
