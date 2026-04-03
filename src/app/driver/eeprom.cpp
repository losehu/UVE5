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

static constexpr uint32_t EEPROM_DIRECT_EEPROM_LIMIT = 0x2000U; // Use only first 8KB on real EEPROM.
static constexpr uint32_t EEPROM_WRITE_READY_TIMEOUT_MS = 120U;
static constexpr uint32_t EEPROM_READY_POLL_INTERVAL_US = 500U;
static constexpr uint32_t EEPROM_POST_WRITE_DELAY_MS = 3U;

static inline bool EEPROM_IsDirectEepromRange(uint32_t address, uint32_t size)
{
    return size > 0U &&
           address < EEPROM_DIRECT_EEPROM_LIMIT &&
           size <= (EEPROM_DIRECT_EEPROM_LIMIT - address);
}

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
#include "../../lib/shared_flash.h"

// ESP32: emulate the radio's EEPROM address space in the internal flash "shared" partition.
//
// Compatibility note:
// - The welcome assets are already mapped by UI code:
//   logical EEPROM [0x02000..0x02FFF] -> shared offset [0x00000..0x00FFF]
// - TLE records were previously mapped in this driver:
//   logical EEPROM [0x1E200..0x1FFFF] -> shared offset [0x10000..0x11DFF]
//
// To keep these existing offsets stable AND still store *all* logical EEPROM addresses
// in the same 512KB partition, we swap the overlapping "shadow" ranges:
// - logical [0x00000..0x00FFF] <-> shared [0x02000..0x02FFF]
// - logical [0x10000..0x11DFF] <-> shared [0x1E200..0x1FFFF]
static constexpr uint32_t EEPROM_LOGICAL_SIZE = 0x80000; // 512KB

static constexpr uint32_t EEPROM_WELCOME_START = 0x02000;
static constexpr uint32_t EEPROM_WELCOME_SIZE  = 0x01000;

static constexpr uint32_t EEPROM_TLE_START     = 0x1E200;
static constexpr uint32_t EEPROM_TLE_END       = 0x20000;
static constexpr uint32_t EEPROM_TLE_SIZE      = EEPROM_TLE_END - EEPROM_TLE_START; // 0x1E00

static constexpr uint32_t SHARED_WELCOME_BASE  = 0x00000;
static constexpr uint32_t SHARED_TLE_WINDOW_BASE = 0x10000;

static inline bool EEPROM_IsWelcomeWindow(uint32_t address)
{
    return address >= EEPROM_WELCOME_START && (address - EEPROM_WELCOME_START) < EEPROM_WELCOME_SIZE;
}

static inline uint32_t EEPROM_WelcomeWindowToSharedOffset(uint32_t address)
{
    return SHARED_WELCOME_BASE + (address - EEPROM_WELCOME_START);
}

static inline bool EEPROM_IsTleWindow(uint32_t address, uint32_t size)
{
    // 0x1E200..0x20000: TLE records (160B each)
    const uint32_t start = EEPROM_TLE_START;
    const uint32_t end = EEPROM_TLE_END;
    return address >= start && (address + size) <= end;
}

static inline uint32_t EEPROM_TleWindowToSharedOffset(uint32_t address)
{
    return SHARED_TLE_WINDOW_BASE + (address - EEPROM_TLE_START);
}

static inline bool EEPROM_IsLowSwapWindow(uint32_t address, uint32_t size)
{
    // Swap logical [0x00000..0x00FFF] into shared [0x02000..0x02FFF]
    const uint32_t start = 0x00000;
    const uint32_t end = 0x01000;
    return address >= start && (address + size) <= end;
}

static inline uint32_t EEPROM_LowSwapToSharedOffset(uint32_t address)
{
    return EEPROM_WELCOME_START + address;
}

static inline bool EEPROM_IsTleSwapWindow(uint32_t address, uint32_t size)
{
    // Swap logical [0x10000..0x11DFF] into shared [0x1E200..0x1FFFF]
    const uint32_t start = SHARED_TLE_WINDOW_BASE;
    const uint32_t end = SHARED_TLE_WINDOW_BASE + EEPROM_TLE_SIZE;
    return address >= start && (address + size) <= end;
}

static inline uint32_t EEPROM_TleSwapToSharedOffset(uint32_t address)
{
    return EEPROM_TLE_START + (address - SHARED_TLE_WINDOW_BASE);
}

static inline uint32_t EEPROM_SharedLogicalSize(void)
{
    const esp_partition_t *p = shared_part();
    if (!p) {
        return 0U;
    }
    const uint32_t sz = (p->size > (size_t)0xFFFFFFFFU) ? 0xFFFFFFFFU : (uint32_t)p->size;
    return (sz < EEPROM_LOGICAL_SIZE) ? sz : EEPROM_LOGICAL_SIZE;
}

static inline bool EEPROM_AddressToSharedOffset(uint32_t address, uint32_t size, uint32_t *outOffset)
{
    const uint32_t logicalSize = EEPROM_SharedLogicalSize();
    if (!outOffset || size == 0U || logicalSize == 0U) {
        return false;
    }
    if (address >= logicalSize || (address + size) > logicalSize) {
        return false;
    }

    if (EEPROM_IsWelcomeWindow(address) && EEPROM_IsWelcomeWindow(address + size - 1U)) {
        *outOffset = EEPROM_WelcomeWindowToSharedOffset(address);
        return true;
    }
    if (EEPROM_IsTleWindow(address, size)) {
        *outOffset = EEPROM_TleWindowToSharedOffset(address);
        return true;
    }
    if (EEPROM_IsLowSwapWindow(address, size)) {
        *outOffset = EEPROM_LowSwapToSharedOffset(address);
        return true;
    }
    if (EEPROM_IsTleSwapWindow(address, size)) {
        *outOffset = EEPROM_TleSwapToSharedOffset(address);
        return true;
    }

    // Default: identity mapping (logical address == shared offset)
    *outOffset = address;
    return true;
}

static inline uint32_t EEPROM_ContiguousSpan(uint32_t address)
{
    const uint32_t logicalSize = EEPROM_SharedLogicalSize();
    if (logicalSize == 0U || address >= logicalSize) {
        return 0U;
    }

    if (EEPROM_IsWelcomeWindow(address)) {
        const uint32_t end = EEPROM_WELCOME_START + EEPROM_WELCOME_SIZE;
        return (end <= logicalSize ? end : logicalSize) - address;
    }
    if (address < 0x01000U) {
        const uint32_t end = 0x01000U;
        return (end <= logicalSize ? end : logicalSize) - address;
    }
    if (EEPROM_IsTleWindow(address, 1U)) {
        const uint32_t end = EEPROM_TLE_END;
        return (end <= logicalSize ? end : logicalSize) - address;
    }
    if (EEPROM_IsTleSwapWindow(address, 1U)) {
        const uint32_t end = SHARED_TLE_WINDOW_BASE + EEPROM_TLE_SIZE;
        return (end <= logicalSize ? end : logicalSize) - address;
    }

    // Identity-mapped region: stop before the next special window begins.
    uint32_t next = logicalSize;
    if (address < EEPROM_WELCOME_START && EEPROM_WELCOME_START < next) {
        next = EEPROM_WELCOME_START;
    }
    if (address < SHARED_TLE_WINDOW_BASE && SHARED_TLE_WINDOW_BASE < next) {
        next = SHARED_TLE_WINDOW_BASE;
    }
    if (address < EEPROM_TLE_START && EEPROM_TLE_START < next) {
        next = EEPROM_TLE_START;
    }
    return next - address;
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
        delayMicroseconds(EEPROM_READY_POLL_INTERVAL_US);
    }
    return false;
}

// EEPROM 初始化
void EEPROM_Init(void) {
    I2C_Init();
}



void EEPROM_ReadBuffer(uint32_t Address, void *pBuffer, uint8_t Size) {

    if (Size == 0U || pBuffer == nullptr) {
        return;
    }

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
    const uint32_t requestSize = (uint32_t)Size;
    const bool directEepromWindow = EEPROM_IsDirectEepromRange(Address, requestSize);

    // Split cross-boundary accesses so [0x0000..0x1FFF] always stays on real EEPROM.
    if (!directEepromWindow &&
        Address < EEPROM_DIRECT_EEPROM_LIMIT &&
        EEPROM_SharedLogicalSize() > 0U) {
        const uint8_t firstChunk = (uint8_t)(EEPROM_DIRECT_EEPROM_LIMIT - Address);
        EEPROM_ReadBuffer(Address, pBuffer, firstChunk);
        EEPROM_ReadBuffer(EEPROM_DIRECT_EEPROM_LIMIT,
                          (uint8_t *)pBuffer + firstChunk,
                          (uint8_t)(Size - firstChunk));
        return;
    }

    if (!directEepromWindow && EEPROM_SharedLogicalSize() > 0U) {
        uint8_t *dst = (uint8_t *)pBuffer;
        uint32_t remaining = requestSize;
        while (remaining > 0U) {
            const uint32_t span = EEPROM_ContiguousSpan(Address);
            uint32_t chunk = span;
            if (chunk == 0U) {
                memset(dst, 0, remaining);
                return;
            }
            if (chunk > remaining) {
                chunk = remaining;
            }

            uint32_t off = 0U;
            if (!EEPROM_AddressToSharedOffset(Address, chunk, &off) ||
                !shared_read((size_t)off, dst, (size_t)chunk)) {
                memset(dst, 0, (size_t)chunk);
            }

            Address += chunk;
            dst += chunk;
            remaining -= chunk;
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
#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
    if (Address >= EEPROM_DIRECT_EEPROM_LIMIT && EEPROM_SharedLogicalSize() > 0U) {
        return Address < EEPROM_SharedLogicalSize();
    }
#endif
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

    if (WRITE_SIZE == 0U || pBuffer == nullptr) {
        return;
    }

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
    const uint32_t requestSize = (uint32_t)WRITE_SIZE;
    const bool directEepromWindow = EEPROM_IsDirectEepromRange(Address, requestSize);

    // Split cross-boundary accesses so [0x0000..0x1FFF] always stays on real EEPROM.
    if (!directEepromWindow &&
        Address < EEPROM_DIRECT_EEPROM_LIMIT &&
        EEPROM_SharedLogicalSize() > 0U) {
        const uint8_t firstChunk = (uint8_t)(EEPROM_DIRECT_EEPROM_LIMIT - Address);
        EEPROM_WriteBuffer(Address, pBuffer, firstChunk);
        EEPROM_WriteBuffer(EEPROM_DIRECT_EEPROM_LIMIT,
                           (const uint8_t *)pBuffer + firstChunk,
                           (uint8_t)(WRITE_SIZE - firstChunk));
        return;
    }

    if (!directEepromWindow && EEPROM_SharedLogicalSize() > 0U) {
        const uint8_t *src = (const uint8_t *)pBuffer;
        uint32_t remaining = requestSize;
        uint8_t buffer[128];

        while (remaining > 0U) {
            const uint32_t span = EEPROM_ContiguousSpan(Address);
            uint32_t chunk = span;
            if (chunk == 0U) {
                return;
            }
            if (chunk > remaining) {
                chunk = remaining;
            }
            if (chunk > sizeof(buffer)) {
                chunk = sizeof(buffer);
            }

            uint32_t off = 0U;
            if (!EEPROM_AddressToSharedOffset(Address, chunk, &off)) {
                return;
            }

            if (!shared_read((size_t)off, buffer, (size_t)chunk)) {
                memset(buffer, 0, (size_t)chunk);
            }
            if (memcmp(src, buffer, (size_t)chunk) != 0) {
                (void)shared_write((size_t)off, src, (size_t)chunk);
            }

            Address += chunk;
            src += chunk;
            remaining -= chunk;
        }
        return;
    }
#endif


    uint8_t buffer[256];
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
            if (EEPROM_POST_WRITE_DELAY_MS > 0U) {
                delay(EEPROM_POST_WRITE_DELAY_MS);
            }
            return;
        }
        I2C_Stop();

        // 写周期 ACK 轮询（比固定 delay 更可靠）
        (void)EEPROM_WaitReady(IIC_ADD, EEPROM_WRITE_READY_TIMEOUT_MS);

        interrupts();
    }
    // 兜底延时，避免某些器件/布线在连续访问时不稳
    if (EEPROM_POST_WRITE_DELAY_MS > 0U) {
        delay(EEPROM_POST_WRITE_DELAY_MS);
    }

}
