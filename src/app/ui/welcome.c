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
#include "../driver/uart1.h"
#include <string.h>
#include "../driver/keyboard.h"
#include "../driver/eeprom.h"
#include "../driver/st7565.h"
#include "../helper/battery.h"
#include "../settings.h"
#include "../misc.h"
#include "helper.h"
#include "welcome.h"
#include "status.h"
#include "../version.h"
#include "../driver/system.h"

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
#include "../shared_flash_c.h"
#endif

static void Welcome_ReadBuffer(uint32_t address, void *buffer, uint8_t size)
{
#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
    // Map the welcome resources from the radio's logical EEPROM window
    // [0x02000..0x02FFF] into ESP32's shared partition (use the first 4KB).
    const uint32_t max = shared_size_c();
    const uint32_t window = (max > 0x1000U) ? 0x1000U : max;
    if (window > 0U && address >= 0x02000) {
        const uint32_t off = address - 0x02000U;
        if (off < window) {
            uint32_t rd = (uint32_t)size;
            if (rd > (window - off)) {
                rd = (window - off);
            }
            if (rd > 0U && !shared_read_c(off, buffer, (size_t)rd)) {
                memset(buffer, 0, (size_t)rd);
            }
            if (rd < (uint32_t)size) {
                memset((uint8_t *)buffer + rd, 0, (size_t)((uint32_t)size - rd));
            }
            return;
        }
    }

    EEPROM_ReadBuffer(address, buffer, size);
#else
    EEPROM_ReadBuffer(address, buffer, size);
#endif
}


void UI_DisplayWelcome(void) {

    char WelcomeString0[19] = {0};
    char WelcomeString1[19] = {0};

    memset(gStatusLine, 0, sizeof(gStatusLine));
    UI_DisplayClear();
    ST7565_BlitStatusLine();  // blank status line
    ST7565_BlitFullScreen();

#if ENABLE_CHINESE_FULL == 4
    if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_MESSAGE) {
        uint8_t welcome_len[2];
        Welcome_ReadBuffer(0x02024, welcome_len, 2);
        welcome_len[0] = welcome_len[0] > 18 ? 0 : welcome_len[0];
        welcome_len[1] = welcome_len[1] > 18 ? 0 : welcome_len[1];
        Welcome_ReadBuffer(0x02000, WelcomeString0, welcome_len[0]);
        Welcome_ReadBuffer(0x02012, WelcomeString1, welcome_len[1]);

        UI_PrintStringSmall(WelcomeString0, 0, 127, 0);
        UI_PrintStringSmall(WelcomeString1, 0, 127, 2);
        sprintf(WelcomeString1, "%u.%02uV %u%%",
                gBatteryVoltageAverage / 100,
                gBatteryVoltageAverage % 100,
                BATTERY_VoltsToPercent(gBatteryVoltageAverage));
        UI_PrintStringSmall(WelcomeString1, 0, 127, 4);
        UI_PrintStringSmall(Version, 0, 127, 6);
    }
    else if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_PIC)
    {
        Welcome_ReadBuffer(0x02080, gStatusLine, 128);
        for (int i = 0; i < 7; ++i) {
            Welcome_ReadBuffer(0x02080 + 128 + 128 * i, &gFrameBuffer[i], 128);
        }
    }

#elif ENABLE_CHINESE_FULL == 0

    Welcome_ReadBuffer(0x0EB0, WelcomeString0, 16);
    Welcome_ReadBuffer(0x0EC0, WelcomeString1, 16);

    UI_PrintStringSmall(WelcomeString0, 0, 127, 0);
    UI_PrintStringSmall(WelcomeString1, 0, 127, 2);
    sprintf(WelcomeString1, "%u.%02uV %u%%",
            gBatteryVoltageAverage / 100,
            gBatteryVoltageAverage % 100,
            BATTERY_VoltsToPercent(gBatteryVoltageAverage));
    UI_PrintStringSmall(WelcomeString1, 0, 127, 4);
    UI_PrintStringSmall(Version, 0, 127, 6);

#endif


    ST7565_BlitStatusLine();  // blank status line
    ST7565_BlitFullScreen();
    BACKLIGHT_TurnOn();


}

