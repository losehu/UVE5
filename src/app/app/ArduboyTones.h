#pragma once

// Compatibility shim for sketches that expect the upstream ArduboyTones library.
// In this project, `ArduboyTones` is provided by our minimal Arduboy2 API shim.

#include "arduboy2.h"

// Minimal pitch constants used by Squid Jump.
// Values are in Hz (only used for compile-time compatibility in this project).
#ifndef NOTE_D5
#define NOTE_D5 587
#endif
#ifndef NOTE_D6
#define NOTE_D6 1175
#endif
#ifndef NOTE_FS6
#define NOTE_FS6 1480
#endif
#ifndef NOTE_D7
#define NOTE_D7 2349
#endif
