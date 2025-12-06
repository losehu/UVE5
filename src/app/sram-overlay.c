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


#include "sram-overlay.h"
#include "../driver/eeprom.h"
static volatile uint32_t *pFlash = 0;
uint32_t                  overlay_FLASH_MainClock;
uint32_t                  overlay_FLASH_ClockMultiplier;
uint32_t                  overlay_0x20000478;         // Nothing is using this???

void overlay_FLASH_RebootToBootloader(void)
{
}

bool overlay_FLASH_IsBusy(void)
{
	return false;
}

bool overlay_FLASH_IsInitComplete(void)
{
	return false;
}

bool overlay_FLASH_IsNotEmpty(void)
{
	return false;
}

void overlay_FLASH_Start(void)
{
}

void overlay_FLASH_Init(FLASH_READ_MODE ReadMode)
{
}

void overlay_FLASH_MaskLock(void)
{
}

void overlay_FLASH_SetMaskSel(FLASH_MASK_SELECTION Mask)
{
}

void overlay_FLASH_MaskUnlock(void)
{
}

void overlay_FLASH_Lock(void)
{
}

void overlay_FLASH_Unlock(void)
{
}

uint32_t overlay_FLASH_ReadByAHB(uint32_t Offset)
{
	return 0;
}

uint32_t overlay_FLASH_ReadByAPB(uint32_t Offset)
{
	return 0;
}

void overlay_FLASH_SetArea(FLASH_AREA Area)
{
}

void overlay_FLASH_SetReadMode(FLASH_READ_MODE Mode)
{
}

void overlay_FLASH_SetEraseTime(void)
{
}

void overlay_FLASH_WakeFromDeepSleep(void)
{
}

void overlay_FLASH_SetMode(FLASH_MODE Mode)
{
}

void overlay_FLASH_SetProgramTime(void)
{
}

void overlay_SystemReset(void)
{
}

uint32_t overlay_FLASH_ReadNvrWord(uint32_t Offset)
{
	return 0;
}

void overlay_FLASH_ConfigureTrimValues(void)
{
}


void ProgramMoreWords(uint32_t DestAddr, const uint32_t *words,uint32_t num)
{
}

void ProgramWords(uint32_t DestAddr,uint32_t words)
{
}

//CP_EEPROM_TO_FLASH(0x5000,0xa000,10*1024);

void CP_EEPROM_TO_FLASH(uint32_t eeprom_add,uint32_t flash_add,uint32_t size)
{
   
}
