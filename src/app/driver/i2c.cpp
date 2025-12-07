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

#include "i2c1.h"
#include <Arduino.h>

// I2C 初始化
void I2C_Init(void) {
    // 配置引脚为输出开漏模式
    pinMode(I2C_PIN_SDA, OUTPUT);
    pinMode(I2C_PIN_SCL, OUTPUT);
    pinMode(I2C_PIN_EN, OUTPUT);
    // 初始化为高电平（空闲状态）
    digitalWrite(I2C_PIN_SDA, HIGH);
    digitalWrite(I2C_PIN_SCL, HIGH);
}

// I2C 开始信号
// SDA 从高变低，而 SCL 保持高
void I2C_Start(void) {
    digitalWrite(I2C_PIN_SDA, HIGH);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SCL, HIGH);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SDA, LOW);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SCL, LOW);
    delayMicroseconds(1);
}

// I2C 停止信号
// SDA 从低变高，而 SCL 为高
void I2C_Stop(void) {
    digitalWrite(I2C_PIN_SDA, LOW);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SCL, LOW);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SCL, HIGH);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SDA, HIGH);
    delayMicroseconds(1);
}

// I2C 读取一个字节
// bFinal: 最后一个字节时为 true（发送 NAK），否则为 false（发送 ACK）
uint8_t I2C_Read(bool bFinal) {
    uint8_t i, Data;
    
    // 配置 SDA 为输入模式（开漏）
    pinMode(I2C_PIN_SDA, INPUT);
    
    Data = 0;
    for (i = 0; i < 8; i++) {
        digitalWrite(I2C_PIN_SCL, LOW);
        delayMicroseconds(1);
        digitalWrite(I2C_PIN_SCL, HIGH);
        delayMicroseconds(1);
        Data <<= 1;
        delayMicroseconds(1);
        if (digitalRead(I2C_PIN_SDA)) {
            Data |= 1U;
        }
        digitalWrite(I2C_PIN_SCL, LOW);
        delayMicroseconds(1);
    }
    
    // 配置 SDA 回到输出模式
    pinMode(I2C_PIN_SDA, OUTPUT);
    digitalWrite(I2C_PIN_SCL, LOW);
    delayMicroseconds(1);
    
    // 发送 ACK 或 NAK
    if (bFinal) {
        digitalWrite(I2C_PIN_SDA, HIGH);  // NAK
    } else {
        digitalWrite(I2C_PIN_SDA, LOW);   // ACK
    }
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SCL, HIGH);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SCL, LOW);
    delayMicroseconds(1);
    
    return Data;
}

// I2C 写入一个字节
// 返回值: 0 表示收到 ACK，-1 表示未收到 ACK
int I2C_Write(uint8_t Data) {
    uint8_t i;
    int ret = -1;
    
    digitalWrite(I2C_PIN_SCL, LOW);
    delayMicroseconds(1);
    
    // 发送 8 位数据
    for (i = 0; i < 8; i++) {
        if ((Data & 0x80) == 0) {
            digitalWrite(I2C_PIN_SDA, LOW);
        } else {
            digitalWrite(I2C_PIN_SDA, HIGH);
        }
        Data <<= 1;
        delayMicroseconds(1);
        digitalWrite(I2C_PIN_SCL, HIGH);
        delayMicroseconds(2);
        digitalWrite(I2C_PIN_SCL, LOW);
        delayMicroseconds(1);
    }
    
    // 读取 ACK 信号
    pinMode(I2C_PIN_SDA, INPUT);
    digitalWrite(I2C_PIN_SCL, LOW);
    delayMicroseconds(1);
    
    for (i = 0; i < 255; i++) {
        if (!digitalRead(I2C_PIN_SDA)) {
            ret = 0;
            break;
        }
    }
    
    digitalWrite(I2C_PIN_SCL, HIGH);
    delayMicroseconds(1);
    digitalWrite(I2C_PIN_SCL, LOW);
    delayMicroseconds(1);
    
    // 恢复 SDA 为输出模式
    pinMode(I2C_PIN_SDA, OUTPUT);
    digitalWrite(I2C_PIN_SDA, HIGH);
    
    return ret;
}

// I2C 读取数据缓冲区
int I2C_ReadBuffer(void *pBuffer, uint8_t Size) {
    uint8_t *pData = (uint8_t *)pBuffer;
    uint8_t i;
    
    if (Size == 1) {
        *pData = I2C_Read(true);
        return 1;
    }
    
    for (i = 0; i < Size - 1; i++) {
        delayMicroseconds(1);
        pData[i] = I2C_Read(false);
    }
    
    delayMicroseconds(1);
    pData[i++] = I2C_Read(true);
    
    return Size;
}

// I2C 写入数据缓冲区
int I2C_WriteBuffer(const void *pBuffer, uint8_t Size) {
    const uint8_t *pData = (const uint8_t *)pBuffer;
    uint8_t i;
    
    for (i = 0; i < Size; i++) {
        if (I2C_Write(*pData++) < 0) {
            return -1;
        }
    }
    
    return 0;
}
