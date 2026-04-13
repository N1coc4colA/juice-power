#ifndef JP_STATES_H
#define JP_STATES_H

#include <cstdint>

enum CommandStates : uint8_t {
    PrepareDrawing = 0b1,
    DrawingPrepared = 0b10,
    Stop = 0b100,
    PauseRendering = 0b1000,

    CommandStates_MIN = PrepareDrawing,
    CommandStates_MAX = Stop,
};

#endif // JP_STATES_H
