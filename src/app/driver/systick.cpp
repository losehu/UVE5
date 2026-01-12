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

#include "systick.h"
#ifndef ENABLE_OPENCV
#include <Arduino.h>
#else
#include "../opencv/Arduino.hpp"
#endif
// ESP32 上使用硬件定时器 API 进行延时

void SYSTICK_Init(void) {
    // 不需要额外初始化，保留接口兼容
}

void SYSTICK_DelayUs(uint32_t Delay) {
    // Arduino 提供的微秒级延时
    delayMicroseconds(Delay);
}

void SYSTICK_Delay250ns(const uint32_t Delay) {
    // 近似实现：在 240MHz 下，1 个周期 ~4.17ns
    // 250ns / 4.17ns ≈ 60 周期，取 62 留裕度
    uint32_t cycles = Delay * 62U;
    while (cycles--) {
        __asm__ __volatile__("nop");
    }
}
