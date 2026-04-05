#ifndef JP_DEFINES_H
#define JP_DEFINES_H

/**
 * @brief Silences unused variable warnings.
 */
template<typename T>
inline constexpr void UNUSED(T)
{}

#include <functional>
#include <mutex>

/**
 * @brief Mutex-protected value wrapper.
 * @tparam T Stored value type.
 */
template<typename T>
class Exclusive
{
public:
    /// @brief Constructs a wrapper with an optional initial value.
    explicit Exclusive(std::mutex &mtx, const T &val = T())
        : m_mtx(mtx)
        , m_value(val)
    {}

    /// @brief Constructs a wrapper from a moved value.
    explicit Exclusive(std::mutex &mtx, T &&val)
        : m_mtx(mtx)
        , m_value(std::move(val))
    {}

    /// @brief Copy constructor.
    explicit Exclusive(const Exclusive &other)
        : m_mtx(other.m_mtx)
        , m_value(other.m_value)
    {}

    /// @brief Move constructor.
    explicit Exclusive(Exclusive &&other) noexcept
        : m_mtx(other.m_mtx)
        , m_value(other.m_value)
    {}

    /// @brief Default destructor.
    ~Exclusive() = default;

    /// @brief Assigns a copied value under lock.
    auto operator=(const T &other) -> Exclusive &
    {
        std::scoped_lock lg(m_mtx.get());
        m_value = other;

        return *this;
    }

    /// @brief Assigns a moved value under lock.
    auto operator=(T &&other) -> Exclusive &
    {
        std::scoped_lock lg(m_mtx.get());
        m_value = std::move(other);

        return *this;
    }

    /// @brief Copy assignment from another wrapper.
    auto operator=(const Exclusive &other) -> Exclusive &
    {
        m_mtx = other.m_mtx;
        m_value = other;

        return *this;
    }

    /// @brief Move assignment from another wrapper.
    auto operator=(Exclusive &&other) noexcept -> Exclusive &
    {
        m_mtx = other.m_mtx;
        m_value = other;

        return *this;
    }

    /// @brief Copies the stored value under lock.
    operator T() const
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    /// @brief Returns mutable value reference protected by lock lifetime.
    auto get() -> T &
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    /// @brief Returns const value reference protected by lock lifetime.
    auto get() const -> const T &
    {
        std::scoped_lock lg(m_mtx);
        return m_value;
    }

    /// @brief Returns mutable value without locking.
    auto unsafeGet() -> T & { return m_value; }
    /// @brief Returns const value without locking.
    auto unsafeGet() const -> const T & { return m_value; }

private:
    /// @brief Referenced mutex used for synchronization.
    mutable std::reference_wrapper<std::mutex> m_mtx;
    /// @brief Stored protected value.
    T m_value;
};

#endif // JP_DEFINES_H
