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

#include "st7565.h"
#ifndef ENABLE_OPENCV
#include <Arduino.h>
#include <SPI.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <driver/gpio.h>
#endif

// 全局帧缓冲区
uint8_t gStatusLine[LCD_WIDTH];
uint8_t gFrameBuffer[FRAME_LINES][LCD_WIDTH];

// ST7565命令定义
static const uint8_t ST7565_CMD_SOFTWARE_RESET = 0xE2;
static const uint8_t ST7565_CMD_BIAS_SELECT = 0xA2;
static const uint8_t ST7565_CMD_COM_DIRECTION = 0xC0;
static const uint8_t ST7565_CMD_SEG_DIRECTION = 0xA0;
static const uint8_t ST7565_CMD_INVERSE_DISPLAY = 0xA6;
static const uint8_t ST7565_CMD_ALL_PIXEL_ON = 0xA4;
static const uint8_t ST7565_CMD_REGULATION_RATIO = 0x20;
static const uint8_t ST7565_CMD_SET_EV = 0x81;
static const uint8_t ST7565_CMD_POWER_CIRCUIT = 0x28;
static const uint8_t ST7565_CMD_SET_START_LINE = 0x40;
static const uint8_t ST7565_CMD_DISPLAY_ON_OFF = 0xAE;

// SPI对象
static SPIClass* spi = nullptr;

// 初始化命令序列
static uint8_t cmds[] = {
    ST7565_CMD_BIAS_SELECT | 0,            // Select bias setting: 1/9
    ST7565_CMD_COM_DIRECTION | (0 << 3),   // Set output direction of COM: normal
    ST7565_CMD_SEG_DIRECTION | 1,          // Set scan direction of SEG: reverse
    ST7565_CMD_INVERSE_DISPLAY | 0,        // Inverse Display: false
    ST7565_CMD_ALL_PIXEL_ON | 0,           // All Pixel ON: false - normal display
    ST7565_CMD_REGULATION_RATIO | (4 << 0),// Regulation Ratio 5.0
    ST7565_CMD_SET_EV,                     // Set contrast
    31,                                     // Contrast value
};

// 内部辅助函数
static inline void CS_LOW() {
#if defined(ARDUINO_ARCH_ESP32)
    gpio_set_level((gpio_num_t)ST7565_PIN_CS, 0);
#else
    digitalWrite(ST7565_PIN_CS, LOW);
#endif
}

static inline void CS_HIGH() {
#if defined(ARDUINO_ARCH_ESP32)
    gpio_set_level((gpio_num_t)ST7565_PIN_CS, 1);
#else
    digitalWrite(ST7565_PIN_CS, HIGH);
#endif
}

static inline void A0_LOW() {
#if defined(ARDUINO_ARCH_ESP32)
    gpio_set_level((gpio_num_t)ST7565_PIN_A0, 0);
#else
    digitalWrite(ST7565_PIN_A0, LOW);
#endif
}

static inline void A0_HIGH() {
#if defined(ARDUINO_ARCH_ESP32)
    gpio_set_level((gpio_num_t)ST7565_PIN_A0, 1);
#else
    digitalWrite(ST7565_PIN_A0, HIGH);
#endif
}

static inline void SPI_BEGIN() {
#if defined(ARDUINO_ARCH_ESP32)
    spi->beginTransaction(SPISettings(ST7565_SPI_FREQ_HZ, MSBFIRST, SPI_MODE0));
#endif
}

static inline void SPI_END() {
#if defined(ARDUINO_ARCH_ESP32)
    spi->endTransaction();
#endif
}

static inline void ST7565_WriteByte_NoCS(uint8_t value) {
    A0_LOW();
    spi->transfer(value);
}

static inline void ST7565_SelectColumnAndLine_NoCS(uint8_t column, uint8_t line) {
    A0_LOW();
    spi->transfer(line + 176);                    // 页地址
    spi->transfer(((column >> 4) & 0x0F) | 0x10); // 列地址高4位
    spi->transfer((column >> 0) & 0x0F);          // 列地址低4位
}

// 硬件复位
void ST7565_HardwareReset(void) {
    digitalWrite(ST7565_PIN_RST, HIGH);
    delay(1);
    digitalWrite(ST7565_PIN_RST, LOW);
    delay(20);
    digitalWrite(ST7565_PIN_RST, HIGH);
    delay(120);
}

// 写入单字节命令
void ST7565_WriteByte(uint8_t value) {
    SPI_BEGIN();
    CS_LOW();
    ST7565_WriteByte_NoCS(value);
    CS_HIGH();
    SPI_END();
}

// 选择列和行
void ST7565_SelectColumnAndLine(uint8_t column, uint8_t line) {
    SPI_BEGIN();
    CS_LOW();
    ST7565_SelectColumnAndLine_NoCS(column, line);
    CS_HIGH();
    SPI_END();
}

// 绘制一行数据 (内部函数)
static void DrawLine(uint8_t column, uint8_t line, const uint8_t *lineBuffer, unsigned size_defVal) {
    SPI_BEGIN();
    CS_LOW();
    ST7565_SelectColumnAndLine_NoCS(column + 4, line);
    A0_HIGH();  // 数据模式
    
    if (lineBuffer) {
        // 发送缓冲区数据
#if defined(ARDUINO_ARCH_ESP32)
        spi->transferBytes(const_cast<uint8_t *>(lineBuffer), nullptr, size_defVal);
#else
        for (unsigned i = 0; i < size_defVal; i++) {
            spi->transfer(lineBuffer[i]);
        }
#endif
    } else {
        // 填充固定值 - 这里的 size_defVal 参数实际表示填充值
        for (unsigned i = 0; i < LCD_WIDTH; i++) {
            spi->transfer((uint8_t)size_defVal);
        }
    }
    
    CS_HIGH();
    SPI_END();
}

// 绘制指定位置的一行
void ST7565_DrawLine(const unsigned int column, const unsigned int line, const uint8_t *pBitmap, const unsigned int size) {
    DrawLine(column, line, pBitmap, size);
}

// 刷新整个屏幕
void ST7565_BlitFullScreen(void) {
    SPI_BEGIN();
    CS_LOW();
    ST7565_WriteByte_NoCS(0x40);  // 设置起始行

    // Pages 1..7 from gFrameBuffer[0..6]
    for (unsigned line = 0; line < FRAME_LINES; line++) {
        ST7565_SelectColumnAndLine_NoCS(0 + 4, static_cast<uint8_t>(line + 1));
        A0_HIGH();
#if defined(ARDUINO_ARCH_ESP32)
        spi->transferBytes(gFrameBuffer[line], nullptr, LCD_WIDTH);
#else
        for (unsigned i = 0; i < LCD_WIDTH; i++) {
            spi->transfer(gFrameBuffer[line][i]);
        }
#endif
    }

    CS_HIGH();
    SPI_END();
}

// 刷新单行
void ST7565_BlitLine(unsigned line) {
    SPI_BEGIN();
    CS_LOW();
    ST7565_WriteByte_NoCS(0x40);  // 设置起始行
    ST7565_SelectColumnAndLine_NoCS(0 + 4, static_cast<uint8_t>(line + 1));
    A0_HIGH();
#if defined(ARDUINO_ARCH_ESP32)
    spi->transferBytes(gFrameBuffer[line], nullptr, LCD_WIDTH);
#else
    for (unsigned i = 0; i < LCD_WIDTH; i++) {
        spi->transfer(gFrameBuffer[line][i]);
    }
#endif
    CS_HIGH();
    SPI_END();
}

// 刷新状态行
void ST7565_BlitStatusLine(void) {
    SPI_BEGIN();
    CS_LOW();
    ST7565_WriteByte_NoCS(0x40);  // 设置起始行
    ST7565_SelectColumnAndLine_NoCS(0 + 4, 0);
    A0_HIGH();
#if defined(ARDUINO_ARCH_ESP32)
    spi->transferBytes(gStatusLine, nullptr, LCD_WIDTH);
#else
    for (unsigned i = 0; i < LCD_WIDTH; i++) {
        spi->transfer(gStatusLine[i]);
    }
#endif
    CS_HIGH();
    SPI_END();
}

void ST7565_BlitAll(void) {
    SPI_BEGIN();
    CS_LOW();

    // Set start line once.
    ST7565_WriteByte_NoCS(0x40);

    // Status line (page 0)
    ST7565_SelectColumnAndLine_NoCS(0 + 4, 0);
    A0_HIGH();
#if defined(ARDUINO_ARCH_ESP32)
    spi->transferBytes(gStatusLine, nullptr, LCD_WIDTH);
#else
    for (unsigned i = 0; i < LCD_WIDTH; i++) {
        spi->transfer(gStatusLine[i]);
    }
#endif

    // Pages 1..7
    for (unsigned line = 0; line < FRAME_LINES; line++) {
        ST7565_SelectColumnAndLine_NoCS(0 + 4, static_cast<uint8_t>(line + 1));
        A0_HIGH();
#if defined(ARDUINO_ARCH_ESP32)
        spi->transferBytes(gFrameBuffer[line], nullptr, LCD_WIDTH);
#else
        for (unsigned i = 0; i < LCD_WIDTH; i++) {
            spi->transfer(gFrameBuffer[line][i]);
        }
#endif
    }

    CS_HIGH();
    SPI_END();
}

// 填充整个屏幕
void ST7565_FillScreen(uint8_t value) {
    for (unsigned i = 0; i < 8; i++) {
        DrawLine(0, i, nullptr, value);
    }
}

// 修复接口故障
void ST7565_FixInterfGlitch(void) {
    for (uint8_t i = 0; i < sizeof(cmds); i++) {
        ST7565_WriteByte(cmds[i]);
    }
}

// 初始化ST7565
void ST7565_Init(void) {
    // 初始化GPIO引脚
    pinMode(ST7565_PIN_RST, OUTPUT);
    pinMode(ST7565_PIN_CS, OUTPUT);
    pinMode(ST7565_PIN_A0, OUTPUT);
    pinMode(ST7565_PIN_MOSI, OUTPUT);
    pinMode(ST7565_PIN_CLK, OUTPUT);
    
    // 默认状态
    CS_HIGH();
    A0_LOW();
    
    // 初始化SPI
    spi = new SPIClass(HSPI);
    spi->begin(ST7565_PIN_CLK, -1, ST7565_PIN_MOSI, ST7565_PIN_CS);
    spi->setFrequency(ST7565_SPI_FREQ_HZ);
    spi->setDataMode(SPI_MODE0);
    spi->setBitOrder(MSBFIRST);
    
    // 硬件复位
    ST7565_HardwareReset();
    
    // 软件复位
    ST7565_WriteByte(ST7565_CMD_SOFTWARE_RESET);
    delay(120);
    
    // 发送初始化命令（前8个）
    for (uint8_t i = 0; i < 8; i++) {
        ST7565_WriteByte(cmds[i]);
    }
    
    // 电源电路初始化序列
    ST7565_WriteByte(ST7565_CMD_POWER_CIRCUIT | 0b011);  // VB=0 VR=1 VF=1
    delay(1);
    ST7565_WriteByte(ST7565_CMD_POWER_CIRCUIT | 0b110);  // VB=1 VR=1 VF=0
    delay(1);
    
    // 重复4次
    for (uint8_t i = 0; i < 4; i++) {
        ST7565_WriteByte(ST7565_CMD_POWER_CIRCUIT | 0b111);  // VB=1 VR=1 VF=1
    }
    
    delay(40);
    
    // 最后的设置
    ST7565_WriteByte(ST7565_CMD_SET_START_LINE | 0);  // 起始行0
    ST7565_WriteByte(ST7565_CMD_DISPLAY_ON_OFF | 1);  // 打开显示
    
    // 清屏
    ST7565_FillScreen(0x00);
}


#else
#include "../opencv/Arduino.hpp"
#endif
