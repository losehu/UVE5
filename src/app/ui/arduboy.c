/* Arduboy UI entry point */
#ifdef ENABLE_ARDUBOY

#include "ui/arduboy.h"

#include "app/arduboy.h"

void UI_DisplayArduboy(void) {
    ARDUBOY_Render();
}

#endif // ENABLE_ARDUBOY
