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

#ifndef DRIVER_I2C_H
#define DRIVER_I2C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// I2C 引脚定义
// SDA -> IO13
// SCL -> IO14
#ifndef I2C_PIN_SDA
#define I2C_PIN_SDA 13
#endif
#ifndef I2C_PIN_SCL
#define I2C_PIN_SCL 14
#endif
#ifndef I2C_PIN_EN
#define I2C_PIN_EN 3
#endif

// I2C 操作类型
enum {
    I2C_WRITE = 0U,
    I2C_READ = 1U,
};

// I2C 函数声明
void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);

uint8_t I2C_Read(bool bFinal);
int I2C_Write(uint8_t Data);

int I2C_ReadBuffer(void *pBuffer, uint8_t Size);
int I2C_WriteBuffer(const void *pBuffer, uint8_t Size);

#ifdef __cplusplus
}
#endif

#endif // DRIVER_I2C_H
