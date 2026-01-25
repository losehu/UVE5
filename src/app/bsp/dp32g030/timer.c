// ESP32 build: provide stubs for the DP32G030 timer/keyboard state used by the TLE module.
// The original firmware updates `my_kbd` from a timer ISR; on ESP32 we update it in the
// TLE loop instead.

#include "timer.h"

KeyboardState my_kbd = {KEY_INVALID, KEY_INVALID, 0, KEY_STATE_IDLE, false};

void TIM0_INIT(void)
{
}

