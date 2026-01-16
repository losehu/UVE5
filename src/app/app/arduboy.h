/* Arduboy mode integration for UVE5 */
#ifndef APP_ARDUBOY_H
#define APP_ARDUBOY_H

#include <stdbool.h>

#include "../driver/keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

void ARDUBOY_Enter(void);
void ARDUBOY_ExitToMain(void);
void ARDUBOY_TimeSlice10ms(void);
void ARDUBOY_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void ARDUBOY_Render(void);

#ifdef ENABLE_SQUID_JUMP
void SquidJump_Init(void);
void SquidJump_Loop(void);
#endif

#ifdef ENABLE_COTD
void Catacombs_Init(void);
void Catacombs_Loop(void);
#endif

#ifdef ENABLE_TRENCH_RUN
void TrenchRun_Init(void);
void TrenchRun_Loop(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
