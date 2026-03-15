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

    Exclusive &operator=(const T &other)
    {
        std::scoped_lock lg(m_mtx.get());
        m_value = other;

        return *this;
    }

    Exclusive &operator=(T &&other)
    {
        std::scoped_lock lg(m_mtx.get());
        m_value = std::move(other);

        return *this;
    }

    Exclusive &operator=(const Exclusive &other)
    {
        m_mtx = other.m_mtx;
        m_value = other;

        return *this;
    }

    Exclusive &operator=(Exclusive &&other) noexcept
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

    T &get()
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    const T &get() const
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    T &unsafeGet() { return m_value; }

    const T &unsafeGet() const { return m_value; }

private:
    mutable std::reference_wrapper<std::mutex> m_mtx;
    T m_value;
};

#endif // JP_DEFINES_H
