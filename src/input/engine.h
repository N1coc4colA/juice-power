#ifndef INPUT_ENGINE_H
#define INPUT_ENGINE_H

#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "src/input/defines.h"

namespace Input {

/**
 * @brief Polls platform events and updates logical input state.
 */
class Engine
{
    /// @brief Pointer-to-member alias for State entries.
    using _StateEntry = State::XS State::*;

public:
    /// @brief Builds the input engine from key-to-event mappings.
    explicit Engine(const std::unordered_map<uint32_t, EventType> &corresps);

    /// @brief Runs the event polling loop.
    void run(std::atomic<uint64_t> &commands);

    /// @brief Returns mutable logical state.
    auto state() -> State & { return m_state; }
    /// @brief Returns const logical state.
    auto state() const -> const State & { return m_state; }

protected:
    /// @brief Current input state snapshot.
    State m_state{};

private:
    /// @brief Set of currently registered pressed keys.
    std::unordered_set<uint32_t> m_registeredKeys{};
    /// @brief Map from platform key code to logical event.
    std::unordered_map<uint32_t, EventType> m_keyToEvent{};
    /// @brief Map from logical event to State member pointer.
    std::unordered_map<EventType, _StateEntry> m_eventToEntry{};
};

} // namespace Input

#endif // INPUT_ENGINE_H
