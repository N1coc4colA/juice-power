#ifndef JP_INPUT_DEFINES_H
#define JP_INPUT_DEFINES_H

#include "src/defines.h"

namespace Input {

/**
 * @brief One logical input button state.
 */
struct StateEntry
{
    /// @brief True while key/button is currently pressed.
    bool state = false;
    /// @brief True while key/button remains held.
    bool hold = false;
} __attribute__((packed));

/**
 * @brief Shared input state container using lock-protected fields.
 */
struct InnerState
{
    /// @brief Alias for synchronized state entry.
    using XS = Exclusive<StateEntry>;

    /// @brief Up action state.
    XS up;
    /// @brief Down action state.
    XS down;
    /// @brief Left action state.
    XS left;
    /// @brief Right action state.
    XS right;
    /// @brief Jump action state.
    XS jump;
    /// @brief Attack action state.
    XS attack;

    /// @brief Builds action states bound to one shared mutex.
    explicit InnerState(std::mutex &m_mtx)
        : up(m_mtx)
        , down(m_mtx)
        , left(m_mtx)
        , right(m_mtx)
        , jump(m_mtx)
        , attack(m_mtx)
    {}
};

/**
 * @brief Owning variant of InnerState with embedded mutex.
 */
struct State : InnerState
{
    /// @brief Mutex protecting all action entries.
    std::mutex m_mtx;
    /// @brief Constructs an empty input state.
    State()
        : InnerState(m_mtx)
    {}
};

/**
 * @brief Logical input actions mapped from platform key codes.
 */
enum class EventType : uint8_t {
    Up, ///< Upward movement.
    Down, ///< Downward movement.
    Left, ///< Left movement.
    Right, ///< Right movement.
    Jump, ///< Jump action.
    Attack, ///< Attack action.
};

} // namespace Input

#endif // JP_INPUT_DEFINES_H
