#ifdef ENABLE_ARDUBOY
#ifdef ENABLE_TRENCH_RUN

// Trench Run integration:
// - build the upstream Arduboy sketch as part of UVE5
// - reuse the global `arduboy` instance from our Arduboy2 shim (see global.h)
// - rename `setup/loop` to callable functions
// - wrap in a namespace to avoid collisions with other integrated sketches.
//   Important: include Arduboy2 shim before entering the namespace so the
//   Arduboy2/Sprites types remain in the global namespace.

#include <Arduino.h>

// Ensure Arduboy2/Sprites are defined in the global namespace.
#include <Arduboy2.h>

#define UVE5_USE_GLOBAL_ARDUBOY 1
#define setup TrenchRun_Init_cpp
#define loop TrenchRun_Loop_cpp

namespace trench_run {
#include "../opencv/game/TrenchRun/TrenchRun.ino"
}

#undef loop
#undef setup

extern "C" void TrenchRun_Init(void) {
    trench_run::TrenchRun_Init_cpp();
}

extern "C" void TrenchRun_Loop(void) {
    trench_run::TrenchRun_Loop_cpp();
}

#endif
#endif
