#ifndef UTILS_H
#define UTILS_H

#include <glm/fwd.hpp>
#include <glm/geometric.hpp>

#include <format>
#include <functional>
#include <iterator>
#include <numeric>
#include <ranges>
#include <type_traits>

#include "defines.h"


namespace utils
{

inline constexpr float cross(const glm::vec2 &a, const glm::vec2 &b)
{
	return (a.x * b.y) - (b.x * a.y);
}

using std::accumulate;

template<auto Member, auto ... Members>
struct access
{
	using type = typename access<Members...>::type;

	template<typename I>
	static inline constexpr auto &&get(I &&instance)
	{
		return access<Members...>::get(instance.*Member);
	}
};

template<auto Member>
struct access<Member>
{
	using type = typename std::remove_reference_t<decltype(Member)>;

	template<typename I>
	static inline constexpr auto &get(I &instance)
	{
		return instance.*Member;
	}
};


template <typename T>
struct is_access : std::false_type {};

template <auto ... Args>
struct is_access<access<Args ...>> : std::true_type {};


template<typename A, typename B>
constexpr bool is_callable(auto V)
{
	return std::is_invocable_v<decltype(V), A, B>;
}


// @0
template<typename T, auto Operation = std::plus<>(), class InputIterator>
inline constexpr T accumulate(InputIterator first, InputIterator last, T init)
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

// @1
template<typename Path, typename T, auto Operation = std::plus<>(), class InputIterator>
requires is_access<Path>::value
inline constexpr T accumulate(InputIterator first, InputIterator last, T init)
{
	for (; first != last; ++first) {
		init = std::invoke(Operation, std::move(init), Path::get(*first));
	}

	return init;
}

//@2
template<typename Path0, typename Path1, typename T, auto Operation = std::plus<>(), auto Composer = std::plus<>(), class InputIterator>
requires is_access<Path0>::value && is_access<Path1>::value
inline constexpr T accumulate(InputIterator first, InputIterator last, T init)
{
	for (; first != last; ++first) {
		init = std::invoke(Operation, std::move(init), std::invoke(Composer, Path0::get(*first), Path1::get(*first)));
	}

	return init;
}

// @3
template<typename Path0, typename Path1, typename T, auto Operation = std::plus<>(), auto Composer = std::plus<>(), class InputIterator0, class InputIterator1>
requires is_access<Path0>::value && is_access<Path1>::value
inline constexpr T accumulate(InputIterator0 first0, InputIterator0 last0, InputIterator1 first1, InputIterator1 last1, T init)
{
	for (; first0 != last0 && first1 != last1; ++first0, ++first1) {
		init = std::invoke(Composer, std::move(init), std::invoke(Operation, Path0::get(*first0), Path1::get(*first1)));
	}

	return init;
}

// @4
template<std::ranges::range Container, typename T, auto Operation = std::plus<>()>
inline constexpr T accumulate(Container &&container, T init)
{
	// Should target @0.
	return accumulate<T, Operation>(
		std::begin(container), std::end(container),
		init
	);
}

// @5
template<typename Path, std::ranges::range Container, typename T, auto Operation = std::plus<>()>
requires is_access<Path>::value
inline constexpr T accumulate(Container &&container, T init)
{
	// Should target @1.
	return accumulate<Path, T, Operation>(
		std::begin(container), std::end(container),
		init
	);
}

//@6
template<typename Path0, typename Path1, std::ranges::range Container, typename T, auto Operation = std::plus<>(), auto Composer = std::plus<>()>
requires is_access<Path0>::value && is_access<Path1>::value
inline constexpr T accumulate(Container &&container, T init)
{
	// Should target @2.
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container), std::end(container),
		init
	);
}

//@7
template<typename Path0, typename Path1, auto Composer = std::plus<>(), auto Operation = std::plus<>(), typename T, std::ranges::range Container>
requires is_access<Path0>::value && is_access<Path1>::value
inline constexpr T accumulate(Container &&container, T init)
{
	// Should target @2.
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container), std::end(container),
		init
	);
}

// @8
template<typename Path0, typename Path1, std::ranges::range Container0, std::ranges::range Container1, typename T, auto Composer = std::plus<>(), auto Operation = std::plus<>()>
requires is_access<Path0>::value && is_access<Path1>::value
inline constexpr T accumulate(Container0 &&container0, Container1 &&container1, T init)
{
	// Should be @3.
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container0), std::end(container0),
		std::begin(container1), std::end(container1),
		init
	);
}

// @9
template<typename Path0, typename Path1, auto Composer = std::plus<>(), auto Operation = std::plus<>(), typename T, std::ranges::range Container0, std::ranges::range Container1>
requires is_access<Path0>::value && is_access<Path1>::value
inline constexpr T accumulate(Container0 &&container0, Container1 &&container1, T init)
{
	// Should be @3.
	return accumulate<Path0, Path1, T, Operation, Composer>(
		std::begin(container0), std::end(container0),
		std::begin(container1), std::end(container1),
		init
	);
}

}

#endif // UTILS_H
