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

#ifndef DRIVER_FLASH_H
#define DRIVER_FLASH_H
#include <stdint.h>

enum FLASH_READ_MODE {
	FLASH_READ_MODE_1_CYCLE = 1,
	FLASH_READ_MODE_2_CYCLE = 2,
};

typedef enum FLASH_READ_MODE FLASH_READ_MODE;

enum FLASH_MASK_SELECTION {
	FLASH_MASK_SELECTION_NONE = 1,
	FLASH_MASK_SELECTION_2KB  = 2,
	FLASH_MASK_SELECTION_4KB  = 3,
	FLASH_MASK_SELECTION_8KB  = 4,
};

typedef enum FLASH_MASK_SELECTION FLASH_MASK_SELECTION;

enum FLASH_MODE {
	FLASH_MODE_READ_AHB = 1,
	FLASH_MODE_PROGRAM  = 2,
	FLASH_MODE_ERASE    = 3,
	FLASH_MODE_READ_APB = 4,
};

typedef enum FLASH_MODE FLASH_MODE;

enum FLASH_AREA {
	FLASH_AREA_MAIN = 1,
	FLASH_AREA_NVR  = 2,
};

typedef enum FLASH_AREA FLASH_AREA;

void FLASH_Init(FLASH_READ_MODE ReadMode);
void FLASH_ConfigureTrimValues(void);
uint32_t FLASH_ReadNvrWord(uint32_t Address);

#endif

