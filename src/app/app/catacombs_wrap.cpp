#ifdef ENABLE_ARDUBOY
#ifdef ENABLE_COTD

// Catacombs of the Damned integration:
// - build the upstream Arduboy sketch and its sources as part of UVE5
// - use the global `arduboy` instance from our Arduboy2 shim
// - rename `setup/loop` to callable functions

#include <Arduino.h>

#define UVE5_USE_GLOBAL_ARDUBOY 1
#define setup Catacombs_Init_cpp
#define loop Catacombs_Loop_cpp

// Compile the sketch entrypoint here; the rest of the game's .cpp files are
// compiled normally by PlatformIO (see build_src_filter in platformio.ini).
#include "../opencv/game/CatacombsOfTheDamned/CatacombsOfTheDamned.ino"

#undef loop
#undef setup

extern "C" void Catacombs_Init(void) {
    Catacombs_Init_cpp();
}

extern "C" void Catacombs_Loop(void) {
    Catacombs_Loop_cpp();
}

#endif
#endif
