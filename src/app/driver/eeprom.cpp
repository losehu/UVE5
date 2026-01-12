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
#ifndef ENABLE_OPENCV
#include "i2c1.h"
#include <Arduino.h>
#else
#include "opencv/Arduino.hpp"
#include <fstream>
#include <vector>
#endif
#include <string.h>

#ifdef ENABLE_OPENCV
namespace {
const char kEepromFile[] = "eeprom.bin";

static void EnsureFileSize(std::fstream &file, std::size_t size) {
    file.seekp(0, std::ios::end);
    std::size_t current = static_cast<std::size_t>(file.tellp());
    if (current >= size) {
        return;
    }
    std::vector<uint8_t> pad(256, 0xFF);
    std::size_t remaining = size - current;
    while (remaining > 0) {
        std::size_t chunk = remaining < pad.size() ? remaining : pad.size();
        file.write(reinterpret_cast<const char *>(pad.data()),
                   static_cast<std::streamsize>(chunk));
        remaining -= chunk;
    }
}

static bool ReadFile(std::fstream &file, uint32_t address, uint8_t *buffer, uint8_t size) {
    file.seekg(0, std::ios::end);
    std::size_t file_size = static_cast<std::size_t>(file.tellg());
    if (address >= file_size) {
        return false;
    }
    file.seekg(static_cast<std::streamoff>(address), std::ios::beg);
    std::size_t available = file_size - address;
    std::size_t to_read = size < available ? size : available;
    file.read(reinterpret_cast<char *>(buffer), static_cast<std::streamsize>(to_read));
    return to_read == size;
}
} // namespace
#endif

// EEPROM 初始化
void EEPROM_Init(void) {
#ifdef ENABLE_OPENCV
    std::fstream file(kEepromFile, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::ofstream create(kEepromFile, std::ios::binary);
    }
#else
    I2C_Init();
#endif
}

void EEPROM_ReadBuffer(uint32_t Address, void *pBuffer, uint8_t Size) {
#ifdef ENABLE_OPENCV
    if (Size == 0) {
        return;
    }
    uint8_t *buffer = static_cast<uint8_t *>(pBuffer);
    memset(buffer, 0xFF, Size);
    std::fstream file(kEepromFile, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return;
    }
    ReadFile(file, Address, buffer, Size);
#else
    noInterrupts();
    I2C_Start();

    uint8_t IIC_ADD =    0xA0 | Address >> 15 &14;

    I2C_Write(IIC_ADD);
    I2C_Write((Address >> 8) & 0xFF);
    I2C_Write((Address >> 0) & 0xFF);

    I2C_Start();

    I2C_Write(IIC_ADD + 1);

    I2C_ReadBuffer(pBuffer, Size);

    I2C_Stop();
    interrupts();
#endif
}

void EEPROM_WriteBuffer(uint32_t Address, const void *pBuffer, uint8_t WRITE_SIZE) {
#ifdef ENABLE_OPENCV
    if (WRITE_SIZE == 0) {
        return;
    }
    std::fstream file(kEepromFile, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::ofstream create(kEepromFile, std::ios::binary);
        if (!create.is_open()) {
            return;
        }
        create.close();
        file.open(kEepromFile, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return;
        }
    }
    std::vector<uint8_t> existing(WRITE_SIZE, 0xFF);
    ReadFile(file, Address, existing.data(), WRITE_SIZE);
    if (memcmp(pBuffer, existing.data(), WRITE_SIZE) != 0) {
        EnsureFileSize(file, static_cast<std::size_t>(Address) + WRITE_SIZE);
        file.seekp(static_cast<std::streamoff>(Address), std::ios::beg);
        file.write(reinterpret_cast<const char *>(pBuffer),
                   static_cast<std::streamsize>(WRITE_SIZE));
        file.flush();
    }
    delay(10);
#else
    uint8_t buffer[128];
    EEPROM_ReadBuffer(Address, buffer, WRITE_SIZE);
    if (memcmp(pBuffer, buffer, WRITE_SIZE) != 0) {
        uint8_t IIC_ADD =    0xA0 | Address >> 15 &14;

        I2C_Start();

        I2C_Write(IIC_ADD);

        I2C_Write((Address >> 8) & 0xFF);
        I2C_Write((Address) & 0xFF);
        I2C_WriteBuffer(pBuffer, WRITE_SIZE);
        I2C_Stop();
    }
    delay(10);
#endif
}
