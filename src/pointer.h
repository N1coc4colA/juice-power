#ifndef JP_POINTER_H
#define JP_POINTER_H

#include "src/keywords.h"

#include <gsl/gsl-lite.hpp>

#include <cstdlib>
#include <iterator>

namespace Private {
/// @brief Signature used by pointer cleanup callbacks.
using Deleter = void(void *);

template<typename T>
/// @brief Default deleter that calls C++ delete.
void typeDelete(const gsl::owner<T *> ptr)
{
    delete ptr;
}

template<typename T>
/// @brief Raw pointer type helper alias.
using PointerType = T *;

} // namespace Private

/**
 * @brief Iterator-like owning pointer wrapper with customizable deleter.
 * @tparam T Pointed element type.
 * @tparam Fn Cleanup function called in destructor.
 */
template<typename T, Private::Deleter Fn>
class AutoCleanPtr
{
public:
    /// @brief Iterator concept used for contiguous iterators.
    using iterator_concept = std::contiguous_iterator_tag;
    /// @brief Legacy iterator category.
    using iterator_category = std::random_access_iterator_tag;
    /// @brief Iterator value type.
    using value_type = T;
    /// @brief Iterator difference type.
    using difference_type = std::ptrdiff_t;
    /// @brief Pointer type.
    using pointer = T *;
    /// @brief Reference type.
    using reference = T &;

    /// @brief Creates a null wrapper.
    AutoCleanPtr()
        : m_ptr(nullptr)
    {}
    /// @brief Wraps a raw pointer.
    explicit AutoCleanPtr(T *ptr)
        : m_ptr(ptr)
    {}
    /// @brief Copies wrapped pointer value.
    AutoCleanPtr(const AutoCleanPtr &other)
        : m_ptr(other.m_ptr)
    {}
    /// @brief Moves wrapped pointer value.
    AutoCleanPtr(AutoCleanPtr &&other) noexcept
        : m_ptr(std::move(other.m_ptr))
    {}
    /// @brief Releases owned pointer with the configured deleter.
    ~AutoCleanPtr() { Fn(gsl::owner<T *>(m_ptr)); }

    /// @brief Copy assignment.
    auto operator=(const AutoCleanPtr &other) -> AutoCleanPtr & = default;
    /// @brief Move assignment.
    auto operator=(AutoCleanPtr &&other) -> AutoCleanPtr & = default;
    /// @brief Assigns a raw pointer.
    auto operator=(T *ptr) -> AutoCleanPtr &
    {
        m_ptr = ptr;
        return *this;
    }

    /// @brief Dereferences the wrapped pointer.
    auto operator*() const -> T & { return *m_ptr; }

    /// @brief Returns the wrapped pointer.
    auto operator->() const -> T * { return m_ptr; }

    /// @brief Random-access element access.
    auto operator[](std::ptrdiff_t pos) const -> T & { return m_ptr[pos]; }

    /// @brief Prefix increment.
    auto operator++() -> AutoCleanPtr &
    {
        ++m_ptr;
        return *this;
    }
    /// @brief Postfix increment.
    auto operator++(int) -> AutoCleanPtr
    {
        auto old = *this;
        ++m_ptr;
        return old;
    }
    /// @brief Prefix decrement.
    auto operator--() -> AutoCleanPtr &
    {
        --m_ptr;
        return *this;
    }
    /// @brief Postfix decrement.
    auto operator--(int) -> AutoCleanPtr
    {
        auto old = *this;
        --m_ptr;
        return old;
    }

    /// @brief Returns iterator advanced by n.
    auto operator+(std::ptrdiff_t n) const -> AutoCleanPtr { return AutoCleanPtr(m_ptr + n); }
    /// @brief Returns iterator moved back by n.
    auto operator-(std::ptrdiff_t n) const -> AutoCleanPtr { return AutoCleanPtr(m_ptr - n); }
    /// @brief Distance between two wrapped pointers.
    auto operator-(const AutoCleanPtr &other) const -> std::ptrdiff_t { return m_ptr - other.m_ptr; }

    /// @brief Symmetric addition operator.
    friend auto operator+(std::ptrdiff_t n, const AutoCleanPtr &it) -> AutoCleanPtr { return AutoCleanPtr(it.m_ptr + n); }

    /// @brief In-place addition.
    auto operator+=(std::ptrdiff_t n) -> AutoCleanPtr &
    {
        m_ptr += n;
        return *this;
    }
    /// @brief In-place subtraction.
    auto operator-=(std::ptrdiff_t n) -> AutoCleanPtr &
    {
        m_ptr -= n;
        return *this;
    }

    /// @brief Three-way comparison on raw pointers.
    auto operator<=>(const AutoCleanPtr &other) const = default;
    /// @brief Equality comparison on raw pointers.
    auto operator==(const AutoCleanPtr &other) const -> bool = default;

    /// @brief Implicit conversion to raw pointer.
    operator T *() const { return m_ptr; }

    /// @brief Returns raw pointer (const overload).
    _nodiscard auto get() const -> const T * { return m_ptr; }
    /// @brief Returns raw pointer (mutable overload).
    _nodiscard auto get() -> T * { return m_ptr; }

private:
    /// @brief Wrapped pointer value.
    T *m_ptr;
};

template<typename T>
/// @brief Pointer wrapper that releases memory with std::free.
using AutoFreePtr = AutoCleanPtr<T, std::free>;
template<typename T>
/// @brief Pointer wrapper that releases memory with delete.
using AutoDeletePtr = AutoCleanPtr<T, Private::typeDelete<T>>;

#endif // JP_POINTER_H
