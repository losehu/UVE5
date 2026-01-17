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

#include "eeprom.h"
#include "i2c1.h"
#include <Arduino.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
#include "../../lib/shared_flash.h"

static inline bool EEPROM_IsWelcomeWindow(uint32_t address)
{
    return address >= 0x02000 && (address - 0x02000) < 0x1000;
}

static inline uint32_t EEPROM_WelcomeWindowToSharedOffset(uint32_t address)
{
    return address - 0x02000;
}
#endif

static inline uint8_t EEPROM_ControlByteWrite(uint32_t Address)
{
    return (uint8_t)(EEPROM_DEVICE_BASE_ADDR | (((Address >> 16) & 0x07U) << 1));
}

static bool EEPROM_WaitReady(uint8_t controlByteWrite, uint32_t timeoutMs)
{
    const uint32_t start = millis();
    while ((uint32_t)(millis() - start) < timeoutMs) {
        I2C_Start();
        const int ack = I2C_Write(controlByteWrite);
        I2C_Stop();
        if (ack == 0) {
            return true;
        }
        delayMicroseconds(200);
    }
    return false;
}

// EEPROM 初始化
void EEPROM_Init(void) {
    I2C_Init();
}



void EEPROM_ReadBuffer(uint32_t Address, void *pBuffer, uint8_t Size) {

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
    if (EEPROM_IsWelcomeWindow(Address)) {
        if (!shared_read(EEPROM_WelcomeWindowToSharedOffset(Address), pBuffer, Size)) {
            memset(pBuffer, 0, Size);
        }
        return;
    }
#endif

    noInterrupts();
    I2C_Start();

    // 24xx 系列控制字节: 1010 A2 A1 A0 R/W
    // 对于 512KB 线性空间（2x256KB），使用 Address[18:16] 映射到 A2..A0
    const uint8_t IIC_ADD = EEPROM_ControlByteWrite(Address);

    if (I2C_Write(IIC_ADD) < 0 ||
        I2C_Write((Address >> 8) & 0xFF) < 0 ||
        I2C_Write((Address >> 0) & 0xFF) < 0) {
        I2C_Stop();
        interrupts();
        return;
    }

    I2C_Start();

    if (I2C_Write(IIC_ADD + 1) < 0) {
        I2C_Stop();
        interrupts();
        return;
    }

    I2C_ReadBuffer(pBuffer, Size);

    I2C_Stop();
    interrupts();

}

bool EEPROM_Probe(uint32_t Address)
{
    noInterrupts();
    I2C_Start();

    const uint8_t iic_add = EEPROM_ControlByteWrite(Address);
    if (I2C_Write(iic_add) < 0) {
        I2C_Stop();
        interrupts();
        return false;
    }

    // 发送 16-bit word address（块内寻址）
    if (I2C_Write((Address >> 8) & 0xFF) < 0 || I2C_Write(Address & 0xFF) < 0) {
        I2C_Stop();
        interrupts();
        return false;
    }

    I2C_Stop();
    interrupts();
    return true;
}

void EEPROM_WriteBuffer(uint32_t Address, const void *pBuffer, uint8_t WRITE_SIZE) {

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
    if (EEPROM_IsWelcomeWindow(Address)) {
        uint8_t buffer[128];
        if (WRITE_SIZE > sizeof(buffer)) {
            return;
        }

        if (!shared_read(EEPROM_WelcomeWindowToSharedOffset(Address), buffer, WRITE_SIZE)) {
            memset(buffer, 0, WRITE_SIZE);
        }
        if (memcmp(pBuffer, buffer, WRITE_SIZE) != 0) {
            (void)shared_write(EEPROM_WelcomeWindowToSharedOffset(Address), pBuffer, WRITE_SIZE);
        }
        return;
    }
#endif


    uint8_t buffer[128];
    EEPROM_ReadBuffer(Address, buffer, WRITE_SIZE);
    if (memcmp(pBuffer, buffer, WRITE_SIZE) != 0) {
        const uint8_t IIC_ADD = EEPROM_ControlByteWrite(Address);

        noInterrupts();

        I2C_Start();

        if (I2C_Write(IIC_ADD) < 0 ||
            I2C_Write((Address >> 8) & 0xFF) < 0 ||
            I2C_Write((Address) & 0xFF) < 0 ||
            I2C_WriteBuffer(pBuffer, WRITE_SIZE) < 0) {
            I2C_Stop();
            interrupts();
            delay(1);
            return;
        }
        I2C_Stop();

        // 写周期 ACK 轮询（比固定 delay 更可靠）
        (void)EEPROM_WaitReady(IIC_ADD, 20);

        interrupts();
    }
    // 兜底延时，避免某些器件/布线在连续访问时不稳

}