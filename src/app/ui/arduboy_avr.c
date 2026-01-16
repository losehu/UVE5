/* Arduboy AVR UI entry point */
#if defined(ENABLE_ARDUBOY_AVR) && (ENABLE_ARDUBOY_AVR)

#include "ui/arduboy_avr.h"

#include "app/arduboy_avr.h"

void UI_DisplayArduboyAvr(void) {
    ARDUBOY_AVR_Render();
}

#endif  // defined(ENABLE_ARDUBOY_AVR) && (ENABLE_ARDUBOY_AVR)
