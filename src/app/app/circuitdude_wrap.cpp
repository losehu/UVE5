#ifdef ENABLE_ARDUBOY
#ifdef ENABLE_CIRCUIT_DUDE

// CircuitDude integration:
// - build the upstream Arduboy sketch as part of UVE5
// - reuse the global `arduboy` instance from our Arduboy2 shim
// - rename `setup/loop` to callable functions
// - wrap in a namespace to avoid collisions with other integrated sketches

#include <Arduino.h>

// Ensure Arduboy2 types are defined in the global namespace.
#include <Arduboy2.h>
#include <Arduboy2Beep.h>
#include <EEPROM.h>

#define UVE5_USE_GLOBAL_ARDUBOY 1
#define UVE5_CIRCUITDUDE_WRAPPED 1
#define setup CircuitDude_Init_cpp
#define loop CircuitDude_Loop_cpp

namespace circuitdude {
#include "circuitdude_prototypes.h"
#include "../opencv/game/CircuitDude/CircuitDude.ino"
}

#undef loop
#undef setup

extern "C" void CircuitDude_Init(void) {
    circuitdude::CircuitDude_Init_cpp();
}

extern "C" void CircuitDude_Loop(void) {
    circuitdude::CircuitDude_Loop_cpp();
}

#endif
#endif
