#ifdef ENABLE_ARDUBOY
#ifdef ENABLE_CRATES3D

// Crates3D integration:
// - build the upstream Arduboy sketch as part of UVE5
// - reuse the global `arduboy` instance from our Arduboy2 shim
// - rename `setup/loop` to callable functions
// - wrap in a namespace to avoid collisions with other integrated sketches

#include <Arduino.h>

#include <Arduboy2.h>
#include <EEPROM.h>

#define UVE5_USE_GLOBAL_ARDUBOY 1
#define UVE5_CRATES3D_WRAPPED 1

#define setup Crates3D_Init_cpp
#define loop Crates3D_Loop_cpp

namespace crates3d {
#include "crates3d_prototypes.h"
#include "../opencv/game/Crates3D/Crates3D.ino"
}

#undef loop
#undef setup

extern "C" void Crates3D_Init(void) {
    crates3d::Crates3D_Init_cpp();
}

extern "C" void Crates3D_Loop(void) {
    crates3d::Crates3D_Loop_cpp();
}

#endif
#endif

