#ifndef INPUT_ENGINE_H
#define INPUT_ENGINE_H

#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "./defines.h"

namespace Input {

class Engine
{
    using _StateEntry = State::XS State::*;

public:
    explicit Engine(const std::unordered_map<uint32_t, EventType> &corresps);

    void run(std::atomic<uint64_t> &commands);

    inline auto state() -> State & { return m_state; }
    inline auto state() const -> const State & { return m_state; }

protected:
    State m_state{};

private:
    std::unordered_set<uint32_t> m_registeredKeys{};
    std::unordered_map<uint32_t, EventType> m_keyToEvent{};
    std::unordered_map<EventType, _StateEntry> m_eventToEntry{};
};

} // namespace Input

#endif // INPUT_ENGINE_H
