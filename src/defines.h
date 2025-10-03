#ifndef JP_DEFINES_H
#define JP_DEFINES_H

#define UNUSED(x) ((void)x)

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
        , m_value(val)
    {}

    explicit Exclusive(const Exclusive &other)
        : m_mtx(other.m_mtx)
        , m_value(other.m_value)
    {}

    explicit Exclusive(Exclusive &&other)
        : m_mtx(other.m_mtx)
        , m_value(other.m_value)
    {}

    void operator=(const T &other)
    {
        std::lock_guard lg(m_mtx.get());
        m_value = other;
    }

    void operator=(T &&other)
    {
        std::lock_guard lg(m_mtx.get());
        m_value = other;
    }

    void operator=(const Exclusive &other)
    {
        m_mtx = other.m_mtx;
        m_value = other;
    }

    void operator=(Exclusive &&other)
    {
        m_mtx = other.m_mtx;
        m_value = other;
    }

    operator T() const
    {
        std::lock_guard lg(m_mtx);
        return m_value;
    }

    T &get()
    {
        std::lock_guard lg(m_mtx);
        return m_value;
    }

    const T &get() const
    {
        std::lock_guard lg(m_mtx);
        return m_value;
    }

    T &unsafeGet() { return m_value; }

    const T &unsafeGet() const { return m_value; }

private:
    mutable std::reference_wrapper<std::mutex> m_mtx;
    T m_value;
};

#endif // JP_DEFINES_H
