#ifndef ALIGNED_VECTOR_H
#define ALIGNED_VECTOR_H

#include <cstddef>
#include <gsl/gsl-lite.hpp>
#include <limits>
#include <memory>
#include <vector>

template<typename T, std::size_t RequiredAlign>
class AlignedAllocator
{
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind
    {
        using other = AlignedAllocator<U, RequiredAlign>;
    };

    constexpr AlignedAllocator() noexcept = default;
    constexpr AlignedAllocator(const AlignedAllocator&) noexcept = default;
    template<typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, RequiredAlign>&) noexcept
    {}

    [[nodiscard]] auto allocate(std::size_t n) -> T *
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length{};
        }

        gsl::owner<void *> ptr = std::aligned_alloc(RequiredAlign, n * sizeof(T));
        if (!ptr) {
            throw std::bad_alloc{};
        }

        return static_cast<T *>(ptr);
    }

    void deallocate(gsl::owner<T *> p, std::size_t) noexcept { std::free(p); }

    template<typename U, std::size_t A>
    auto operator==(const AlignedAllocator<U, A>&) const noexcept -> bool
    {
        return RequiredAlign == A;
    }

    template<typename U, std::size_t A>
    auto operator!=(const AlignedAllocator<U, A>&) const noexcept -> bool
    {
        return !(*this == AlignedAllocator<U, A>{});
    }
};

template<typename T, std::size_t RequiredAlign>
using AlignedVector = std::vector<T, AlignedAllocator<T, RequiredAlign>>;

template<typename T, typename A>
using TypeAlignedVector = AlignedVector<T, alignof(A)>;

#endif // ALIGNED_VECTOR_H
