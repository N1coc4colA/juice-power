#ifndef INPUT_DEFINES_H
#define INPUT_DEFINES_H

#include "../defines.h"

namespace Input {

struct StateEntry
{
    bool state = false;
    bool hold = false;
} __attribute__((packed));

struct InnerState
{
    using XS = Exclusive<StateEntry>;

    XS up;
    XS down;
    XS left;
    XS right;
    XS jump;
    XS attack;

    explicit InnerState(std::mutex &m_mtx)
        : up(m_mtx)
        , down(m_mtx)
        , left(m_mtx)
        , right(m_mtx)
        , jump(m_mtx)
        , attack(m_mtx)
    {}
};

struct State : public InnerState
{
    std::mutex m_mtx;

public:
    inline State()
        : InnerState(m_mtx)
    {}
};

enum EventType {
    Up,
    Down,
    Left,
    Right,
    Jump,
    Attack,
};

} // namespace Input

#endif // INPUT_DEFINES_H
