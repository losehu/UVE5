#ifdef ENABLE_ARDUBOY
#ifdef ENABLE_SQUID_JUMP

// Build the upstream Arduboy sketch as part of UVE5, but:
// - use the global `arduboy` instance from our shim
// - rename `setup/loop` to callable functions
// - avoid clashing with the POSIX `pause()` symbol on ESP32/newlib
// - add explicit forward declarations (Arduino `.ino` normally auto-generates them)

#include <Arduino.h>
#include <math.h>

#define UVE5_USE_GLOBAL_ARDUBOY 1
#define setup SquidJump_Init_cpp
#define loop SquidJump_Loop_cpp
#define pause SquidJump_Pause_cpp

// Types used by the sketch prototypes.
#include "../opencv/game/squid_jump/Structs.hpp"
#include "squid_jump_prototypes.h"

#include "../opencv/game/squid_jump/squid_jump.ino"

#undef pause
#undef loop
#undef setup

extern "C" void SquidJump_Init(void) {
	SquidJump_Init_cpp();
}

extern "C" void SquidJump_Loop(void) {
	SquidJump_Loop_cpp();
}

#endif
#endif

