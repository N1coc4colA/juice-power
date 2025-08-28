#ifndef INPUT_ENGINE_H
#define INPUT_ENGINE_H

#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "./defines.h"

namespace Input {

class Engine
{
    typedef State::XS State::*_StateEntry;

public:
    explicit Engine(const std::unordered_map<uint32_t, EventType> &corresps);

    void run(std::atomic<uint64_t> *commands);

    inline State &state() { return m_state; }
    inline const State &state() const { return m_state; }

protected:
    State m_state{};

private:
    std::unordered_set<uint32_t> m_registeredKeys{};
    std::unordered_map<uint32_t, EventType> m_keyToEvent{};
    std::unordered_map<EventType, _StateEntry> m_eventToEntry{};
};

} // namespace Input

#endif // INPUT_ENGINE_H
