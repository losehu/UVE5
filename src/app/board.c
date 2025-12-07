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

#include <string.h>

#ifdef ENABLE_FMRADIO
#include "app/fm.h"
#endif

#include "board.h"
#include "driver/gpio.h"

#include "driver/adc1.h"
#include "driver/backlight.h"

#include "sram-overlay.h"
#include "driver/eeprom.h"
#ifdef ENABLE_FMRADIO
#include "driver/bk1080.h"
#endif

#include "driver/crc.h"
#include "driver/eeprom.h"
#include "driver/flash.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/st7565.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "settings.h"

#if defined(ENABLE_OVERLAY)
#include "sram-overlay.h"
#endif

#if defined(ENABLE_OVERLAY)
void BOARD_FLASH_Init(void)
    {
        FLASH_Init(FLASH_READ_MODE_1_CYCLE);
        FLASH_ConfigureTrimValues();
        SYSTEM_ConfigureClocks();

        overlay_FLASH_MainClock       = 48000000;
        overlay_FLASH_ClockMultiplier = 48;

        FLASH_Init(FLASH_READ_MODE_2_CYCLE);
    }
#endif

void BOARD_GPIO_Init(void) {
   
}

void BOARD_PORTCON_Init(void) {
  
}

void BOARD_ADC_Init(void) {

    ADC_Configure();
    ADC_Enable();
    ADC_SoftReset();
}

void BOARD_ADC_GetBatteryInfo(uint16_t *pVoltage, uint16_t *pCurrent) {
    ADC_Start();
    while (!ADC_CheckEndOfConversion(ADC_CH9)) {}
    *pVoltage = ADC_GetValue(ADC_CH4);
    *pCurrent = ADC_GetValue(ADC_CH9);
}

void BOARD_Init(void) {
    BOARD_PORTCON_Init();
    BOARD_GPIO_Init();
    BACKLIGHT_InitHardware();
    BOARD_ADC_Init();
    ST7565_Init();
#ifdef ENABLE_FMRADIO
    BK1080_Init(0, false);
#endif

#if defined(ENABLE_UART) || defined(ENABLED_AIRCOPY)
    CRC_Init();
#endif

}

void write_to_memory(uint32_t address, uint32_t data) {

}
//JUMP_TO_FLASH(0xa10A,0x20003ff0);
void JUMP_TO_FLASH(uint32_t flash_add,uint32_t stack_add)
{
   
}