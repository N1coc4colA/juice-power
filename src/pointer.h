#ifndef POINTER_H
#define POINTER_H

#include "keywords.h"

#include <cstdlib>
#include <gsl/gsl-lite.hpp>
#include <iterator>

namespace _priv {
using Deleter = void(void *);

template<typename T>
void typeDelete(gsl::owner<T *> ptr)
{
    delete ptr;
}

template<typename T>
using PointerType = T *;

} // namespace _priv

template<typename T, _priv::Deleter Fn>
class AutoCleanPtr
{
public:
    // --- Iterator traits (required for contiguous_iterator) ---
    using iterator_concept = std::contiguous_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    AutoCleanPtr()
        : m_ptr(nullptr)
    {} // default-constructible is needed too
    AutoCleanPtr(T *ptr)
        : m_ptr(ptr)
    {}
    AutoCleanPtr(const AutoCleanPtr &other)
        : m_ptr(other.m_ptr)
    {}
    AutoCleanPtr(AutoCleanPtr &&other) noexcept
        : m_ptr(std::move(other.m_ptr))
    {}
    ~AutoCleanPtr() { Fn(m_ptr); }

    auto operator=(const AutoCleanPtr &other) -> AutoCleanPtr & = default;
    auto operator=(AutoCleanPtr &&other) -> AutoCleanPtr & = default;
    auto operator=(T *ptr) -> AutoCleanPtr &
    {
        m_ptr = ptr;
        return *this;
    }

    // Dereference (required)
    auto operator*() const -> T & { return *m_ptr; }

    // Arrow (must return raw pointer for contiguity)
    auto operator->() const -> T * { return m_ptr; }

    // Subscript
    auto operator[](std::ptrdiff_t pos) const -> T & { return m_ptr[pos]; }

    // Increment / decrement
    auto operator++() -> AutoCleanPtr &
    {
        ++m_ptr;
        return *this;
    }
    auto operator++(int) -> AutoCleanPtr
    {
        auto old = *this;
        ++m_ptr;
        return old;
    }
    auto operator--() -> AutoCleanPtr &
    {
        --m_ptr;
        return *this;
    }
    auto operator--(int) -> AutoCleanPtr
    {
        auto old = *this;
        --m_ptr;
        return old;
    }

    // Arithmetic (required for random_access_iterator)
    auto operator+(std::ptrdiff_t n) const -> AutoCleanPtr { return AutoCleanPtr(m_ptr + n); }
    auto operator-(std::ptrdiff_t n) const -> AutoCleanPtr { return AutoCleanPtr(m_ptr - n); }
    auto operator-(const AutoCleanPtr &other) const -> std::ptrdiff_t { return m_ptr - other.m_ptr; }

    friend auto operator+(std::ptrdiff_t n, const AutoCleanPtr &it) -> AutoCleanPtr { return AutoCleanPtr(it.m_ptr + n); }

    auto operator+=(std::ptrdiff_t n) -> AutoCleanPtr &
    {
        m_ptr += n;
        return *this;
    }
    auto operator-=(std::ptrdiff_t n) -> AutoCleanPtr &
    {
        m_ptr -= n;
        return *this;
    }

    // Comparison (required; <=> gives all six for free)
    auto operator<=>(const AutoCleanPtr &other) const = default;
    auto operator==(const AutoCleanPtr &other) const -> bool = default;

    // Raw pointer access (used by std::to_address / span internals)
    operator T *() const { return m_ptr; }

    _nodiscard auto get() const -> const T * { return m_ptr; }
    _nodiscard auto get() -> T * { return m_ptr; }

private:
    T *m_ptr;
};

template<typename T>
using AutoFreePtr = AutoCleanPtr<T, std::free>;
template<typename T>
using AutoDeletePtr = AutoCleanPtr<T, _priv::typeDelete<T>>;

#endif // POINTER_H
