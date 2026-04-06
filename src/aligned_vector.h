#ifndef ALIGNED_VECTOR_H
#define ALIGNED_VECTOR_H

#include <gsl/gsl-lite.hpp>

#include <cstddef>
#include <limits>
#include <vector>

#include "src/keywords.h"

/**
 * @brief STL-compatible allocator using std::aligned_alloc.
 * @tparam T Allocated element type.
 * @tparam RequiredAlign Required byte alignment.
 */
template<typename T, std::size_t RequiredAlign>
class AlignedAllocator
{
public:
    /// @brief Allocated value type.
    using value_type = T;
    /// @brief Mutable pointer type.
    using pointer = T*;
    /// @brief Const pointer type.
    using const_pointer = const T*;
    /// @brief Mutable reference type.
    using reference = T&;
    /// @brief Const reference type.
    using const_reference = const T&;
    /// @brief Unsigned size type.
    using size_type = std::size_t;
    /// @brief Signed difference type.
    using difference_type = std::ptrdiff_t;

    template<typename U>
    /// @brief Rebind helper used by allocator_traits.
    struct rebind
    {
        /// @brief Rebound allocator type for a different element type.
        using other = AlignedAllocator<U, RequiredAlign>;
    };

    /// @brief Default allocator constructor.
    constexpr AlignedAllocator() noexcept = default;
    /// @brief Copy constructor.
    constexpr AlignedAllocator(const AlignedAllocator&) noexcept = default;
    /// @brief Converting copy constructor.
    template<typename U>
    explicit constexpr AlignedAllocator(const AlignedAllocator<U, RequiredAlign>&) noexcept
    {}
    AlignedAllocator(AlignedAllocator &&) noexcept {}
    ~AlignedAllocator() = default;

    auto operator=(const AlignedAllocator &) noexcept -> AlignedAllocator & = default;
    auto operator=(AlignedAllocator &&) noexcept -> AlignedAllocator & { return *this; };

    /// @brief Allocates memory for n elements.
    _nodiscard static auto allocate(const std::size_t n) -> T *
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

    /// @brief Frees memory allocated by allocate().
    static void deallocate(const gsl::owner<T *> p, std::size_t) noexcept { std::free(p); }

    /// @brief Allocator equality by alignment value.
    template<typename U, std::size_t A>
    auto operator==(const AlignedAllocator<U, A>&) const noexcept -> bool
    {
        return RequiredAlign == A;
    }

    /// @brief Allocator inequality by alignment value.
    template<typename U, std::size_t A>
    auto operator!=(const AlignedAllocator<U, A>&) const noexcept -> bool
    {
        return !(*this == AlignedAllocator<U, A>{});
    }
};

template<typename T, std::size_t RequiredAlign>
/// @brief Vector alias backed by the aligned allocator.
using AlignedVector = std::vector<T, AlignedAllocator<T, RequiredAlign>>;

template<typename T, typename A>
/// @brief Vector aligned on the alignment of type A.
using TypeAlignedVector = AlignedVector<T, alignof(A)>;

#endif // ALIGNED_VECTOR_H
