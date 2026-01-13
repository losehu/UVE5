/* Arduboy AVR emulator integration for UVE5 */
#ifndef APP_ARDUBOY_AVR_H
#define APP_ARDUBOY_AVR_H

#include <stdbool.h>

#include "../driver/keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

void ARDUBOY_AVR_Enter(void);
void ARDUBOY_AVR_ExitToMain(void);
void ARDUBOY_AVR_TimeSlice10ms(void);
void ARDUBOY_AVR_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void ARDUBOY_AVR_Render(void);

#ifdef __cplusplus
}
#endif

#endif
