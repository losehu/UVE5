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

#ifndef DRIVER_ST7565_H
#define DRIVER_ST7565_H

#include <stdint.h>
#include <stdbool.h>

// 引脚定义
// RST IO45
// CS IO7
// MOSI IO4
// CLK IO6
// A0 IO5
#define ST7565_PIN_RST  45
#define ST7565_PIN_CS   7
#define ST7565_PIN_MOSI 4
#define ST7565_PIN_CLK  6
#define ST7565_PIN_A0   5

// LCD尺寸定义
#define LCD_WIDTH       128
#define LCD_HEIGHT       64
#define FRAME_LINES     7

// 全局帧缓冲区
extern uint8_t gStatusLine[LCD_WIDTH];
extern uint8_t gFrameBuffer[FRAME_LINES][LCD_WIDTH];

// 公共函数接口
void ST7565_Init(void);
void ST7565_HardwareReset(void);
void ST7565_WriteByte(uint8_t value);
void ST7565_SelectColumnAndLine(uint8_t column, uint8_t line);
void ST7565_DrawLine(const unsigned int column, const unsigned int line, const uint8_t *pBitmap, const unsigned int size);
void ST7565_BlitFullScreen(void);
void ST7565_BlitLine(unsigned line);
void ST7565_BlitStatusLine(void);
void ST7565_FillScreen(uint8_t value);
void ST7565_FixInterfGlitch(void);

#endif // DRIVER_ST7565_H
