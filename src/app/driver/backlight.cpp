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

#include "backlight.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "../settings.h"

// this is decremented once every 500ms
uint16_t gBacklightCountdown_500ms = 0;
bool backlightOn;

void BACKLIGHT_InitHardware() {
    pinMode(BACKLIGHT_IO, OUTPUT);
}
unsigned short BACKLIGHT_MAP[7]={11,21,41,121,241,481,0};

void BACKLIGHT_TurnOn(void) {
    if (gEeprom.BACKLIGHT_TIME == 0) {
        BACKLIGHT_TurnOff();
        return;
    }

    backlightOn = true;
    BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MAX);
    gBacklightCountdown_500ms = BACKLIGHT_MAP[gEeprom.BACKLIGHT_TIME-1];

}

void BACKLIGHT_TurnOff() {
    BACKLIGHT_SetBrightness(0);
    gBacklightCountdown_500ms = 0;
    backlightOn = false;
}

bool BACKLIGHT_IsOn() {
    return backlightOn;
}

static uint8_t currentBrightness;

void BACKLIGHT_SetBrightness(uint8_t brigtness) {
    const uint8_t value[]= {0,3,6,9,15,24,38,62,100,159,255};
    currentBrightness = brigtness;
    analogWrite(BACKLIGHT_IO, value[brigtness]);
}

uint8_t BACKLIGHT_GetBrightness(void) {
    return currentBrightness;
}