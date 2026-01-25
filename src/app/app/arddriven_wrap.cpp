#ifdef ENABLE_ARDUBOY
#ifdef ENABLE_ARDDRIVEN

// ArdDriven integration:
// - build the upstream Arduboy sketch as part of UVE5
// - reuse the global `arduboy` instance from our Arduboy2 shim
// - rename `setup/loop` to callable functions
// - wrap in a namespace to avoid collisions with other integrated sketches

#include <Arduino.h>

// Ensure Arduboy2/Sprites are defined in the global namespace.
#include <Arduboy2.h>
#include <ArduboyTones.h>

#define UVE5_USE_GLOBAL_ARDUBOY 1
#define setup ArdDriven_Init_cpp
#define loop ArdDriven_Loop_cpp

namespace arddriven {
#include "../opencv/game/ArdDriven/ArdDriven.ino"
}

#undef loop
#undef setup

extern "C" void ArdDriven_Init(void) {
    arddriven::ArdDriven_Init_cpp();
}

extern "C" void ArdDriven_Loop(void) {
    arddriven::ArdDriven_Loop_cpp();
}

#endif
#endif

