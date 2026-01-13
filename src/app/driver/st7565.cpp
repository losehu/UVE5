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
    digitalWrite(ST7565_PIN_CS, LOW);
}

static inline void CS_HIGH() {
    digitalWrite(ST7565_PIN_CS, HIGH);
}

static inline void A0_LOW() {
    digitalWrite(ST7565_PIN_A0, LOW);
}

static inline void A0_HIGH() {
    digitalWrite(ST7565_PIN_A0, HIGH);
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
    A0_LOW();  // 命令模式
    CS_LOW();
    spi->transfer(value);
    CS_HIGH();
}

// 选择列和行
void ST7565_SelectColumnAndLine(uint8_t column, uint8_t line) {
    A0_LOW();  // 命令模式
    CS_LOW();
    spi->transfer(line + 176);                    // 页地址
    spi->transfer(((column >> 4) & 0x0F) | 0x10); // 列地址高4位
    spi->transfer((column >> 0) & 0x0F);          // 列地址低4位
    CS_HIGH();
}

// 绘制一行数据 (内部函数)
static void DrawLine(uint8_t column, uint8_t line, const uint8_t *lineBuffer, unsigned size_defVal) {
    ST7565_SelectColumnAndLine(column + 4, line);
    
    A0_HIGH();  // 数据模式
    CS_LOW();
    
    if (lineBuffer) {
        // 发送缓冲区数据
        for (unsigned i = 0; i < size_defVal; i++) {
            spi->transfer(lineBuffer[i]);
        }
    } else {
        // 填充固定值 - 这里的 size_defVal 参数实际表示填充值
        for (unsigned i = 0; i < LCD_WIDTH; i++) {
            spi->transfer((uint8_t)size_defVal);
        }
    }
    
    CS_HIGH();
}

// 绘制指定位置的一行
void ST7565_DrawLine(const unsigned int column, const unsigned int line, const uint8_t *pBitmap, const unsigned int size) {
    DrawLine(column, line, pBitmap, size);
}

// 刷新整个屏幕
void ST7565_BlitFullScreen(void) {
    ST7565_WriteByte(0x40);  // 设置起始行
    
    for (unsigned line = 0; line < FRAME_LINES; line++) {
        DrawLine(0, line + 1, gFrameBuffer[line], LCD_WIDTH);
    }
}

// 刷新单行
void ST7565_BlitLine(unsigned line) {
    ST7565_WriteByte(0x40);  // 设置起始行
    DrawLine(0, line + 1, gFrameBuffer[line], LCD_WIDTH);
}

// 刷新状态行
void ST7565_BlitStatusLine(void) {
    ST7565_WriteByte(0x40);  // 设置起始行
    DrawLine(0, 0, gStatusLine, LCD_WIDTH);
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
    digitalWrite(ST7565_PIN_CS, HIGH);
    digitalWrite(ST7565_PIN_A0, LOW);
    
    // 初始化SPI
    spi = new SPIClass(HSPI);
    spi->begin(ST7565_PIN_CLK, -1, ST7565_PIN_MOSI, ST7565_PIN_CS);
    spi->setFrequency(4000000);  // 4MHz SPI时钟
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
