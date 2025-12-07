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

#ifndef DRIVER_EEPROM_H
#define DRIVER_EEPROM_H

#include <stdint.h>

// EEPROM 设备地址
// 芯片通常使用 A0, A1, A2 三个地址引脚来扩展地址空间
#define EEPROM_DEVICE_BASE_ADDR 0xA0

#ifdef __cplusplus
extern "C" {
#endif

// EEPROM 操作函数
void EEPROM_Init(void);
void EEPROM_ReadBuffer(uint32_t Address, void *pBuffer, uint8_t Size);
void EEPROM_WriteBuffer(uint32_t Address, const void *pBuffer, uint8_t Size);

#ifdef __cplusplus
}
#endif

#endif // DRIVER_EEPROM_H
