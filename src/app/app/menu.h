/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef APP_MENU_H
#define APP_MENU_H

#include "../driver/keyboard.h"
#include "../driver/pcf8563.h"

#ifndef ENABLE_MENU_TEST_MODE
#define ENABLE_MENU_TEST_MODE 1
#endif

#ifdef ENABLE_F_CAL_MENU
void writeXtalFreqCal(const int32_t value, const bool update_eeprom);
#endif

//extern uint8_t gUnlockAllTxConfCnt;

int MENU_GetLimits(uint8_t menu_id, int32_t *pMin, int32_t *pMax);

void MENU_AcceptSetting(void);

void MENU_ShowCurrentSetting(void);

void MENU_StartCssScan(void);

void MENU_CssScanFound(void);

void MENU_StopCssScan(void);

void MENU_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

// RTC menu helper: returns current edit field (0..5) when editing RTC menu.
uint8_t MENU_RTC_GetField(void);

// RTC menu helper: returns the time that should be displayed for RTC menu.
// When editing, this is the in-memory editable copy; otherwise it reads from RTC.
bool MENU_RTC_GetDisplayTime(pcf8563_time_t *t);

typedef struct {
    double lat;
    double lon;
    double height;
} menu_location_t;

// Location menu helper: returns selected field (0:lat,1:lon,2:height) when editing.
uint8_t MENU_LOC_GetField(void);

// Location menu helper: returns the location that should be displayed for LOC menu.
// When editing, this is the in-memory editable copy; otherwise it reads from EEPROM.
bool MENU_LOC_GetDisplay(menu_location_t *loc);

// Location menu helper: returns the current input buffer (only when editing and typing),
// or NULL when not actively entering a value.
const char *MENU_LOC_GetInput(void);

#if ENABLE_MENU_TEST_MODE
const char *MENU_TEST_GetStatusText(void);
#endif


#endif

