#ifndef JP_DEFINES_H
#define JP_DEFINES_H

template<typename T>
inline constexpr void UNUSED(T)
{}

#include <functional>
#include <mutex>

template<typename T>
class Exclusive
{
public:
    explicit Exclusive(std::mutex &mtx, const T &val = T())
        : m_mtx(mtx)
        , m_value(val)
    {}

    explicit Exclusive(std::mutex &mtx, T &&val)
        : m_mtx(mtx)
        , m_value(std::move(val))
    {}

    explicit Exclusive(const Exclusive &other)
        : m_mtx(other.m_mtx)
        , m_value(other.m_value)
    {}

    explicit Exclusive(Exclusive &&other) noexcept
        : m_mtx(other.m_mtx)
        , m_value(other.m_value)
    {}

    ~Exclusive() = default;

    auto operator=(const T &other) -> Exclusive &
    {
        std::scoped_lock lg(m_mtx.get());
        m_value = other;

        return *this;
    }

    auto operator=(T &&other) -> Exclusive &
    {
        std::scoped_lock lg(m_mtx.get());
        m_value = std::move(other);

        return *this;
    }

    auto operator=(const Exclusive &other) -> Exclusive &
    {
        m_mtx = other.m_mtx;
        m_value = other;

        return *this;
    }

    auto operator=(Exclusive &&other) noexcept -> Exclusive &
    {
        m_mtx = other.m_mtx;
        m_value = other;

        return *this;
    }

    operator T() const
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    auto get() -> T &
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    auto get() const -> const T &
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    auto unsafeGet() -> T & { return m_value; }
    auto unsafeGet() const -> const T & { return m_value; }

private:
    mutable std::reference_wrapper<std::mutex> m_mtx;
    T m_value;
};

#endif // JP_DEFINES_H
