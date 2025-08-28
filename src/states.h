#ifndef STATES_H
#define STATES_H

#include <cstdint>

enum CommandStates : uint64_t {
    PrepareDrawing = 0b1,
    DrawingPrepared = 0b10,
    Stop = 0b100,
    PauseRendering = 0b1000,

    CommandStates_MIN = PrepareDrawing,
    CommandStates_MAX = Stop,
};

#endif // STATES_H
