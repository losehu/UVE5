#include "pcf8563.h"

#include "i2c1.h"
#include <Arduino.h>

static inline uint8_t pcf8563_addr_write(void)
{
    return (uint8_t)((PCF8563_I2C_ADDR_7BIT << 1) | 0U);
}

static inline uint8_t pcf8563_addr_read(void)
{
    return (uint8_t)((PCF8563_I2C_ADDR_7BIT << 1) | 1U);
}

static inline uint8_t bcd2dec(uint8_t v)
{
    return (uint8_t)(((v >> 4) * 10U) + (v & 0x0FU));
}

static inline uint8_t dec2bcd(uint8_t v)
{
    return (uint8_t)(((v / 10U) << 4) | (v % 10U));
}

bool PCF8563_Probe(void)
{
    const uint8_t addr_w = pcf8563_addr_write();

    noInterrupts();
    I2C_Start();
    const int ack = I2C_Write(addr_w);
    I2C_Stop();
    interrupts();

    return ack == 0;
}

bool PCF8563_ReadRaw(uint8_t reg, void *buf, uint8_t len)
{
    if (!buf || len == 0) {
        return false;
    }

    const uint8_t addr_w = pcf8563_addr_write();
    const uint8_t addr_r = pcf8563_addr_read();

    noInterrupts();

    I2C_Start();
    if (I2C_Write(addr_w) < 0 || I2C_Write(reg) < 0) {
        I2C_Stop();
        interrupts();
        return false;
    }

    I2C_Start();
    if (I2C_Write(addr_r) < 0) {
        I2C_Stop();
        interrupts();
        return false;
    }

    I2C_ReadBuffer(buf, len);
    I2C_Stop();

    interrupts();
    return true;
}

bool PCF8563_WriteRaw(uint8_t reg, const void *buf, uint8_t len)
{
    if (!buf || len == 0) {
        return false;
    }

    const uint8_t addr_w = pcf8563_addr_write();

    noInterrupts();

    I2C_Start();
    if (I2C_Write(addr_w) < 0 || I2C_Write(reg) < 0 || I2C_WriteBuffer(buf, len) < 0) {
        I2C_Stop();
        interrupts();
        return false;
    }

    I2C_Stop();
    interrupts();
    return true;
}

bool PCF8563_ReadTime(pcf8563_time_t *t)
{
    if (!t) {
        return false;
    }

    // Registers: 0x02..0x08
    // 0x02 VL/seconds, 0x03 minutes, 0x04 hours, 0x05 days,
    // 0x06 weekdays, 0x07 century/months, 0x08 years
    uint8_t r[7] = {0};
    if (!PCF8563_ReadRaw(0x02, r, sizeof(r))) {
        return false;
    }

    const bool vl = (r[0] & 0x80U) != 0;
    const uint8_t sec = bcd2dec((uint8_t)(r[0] & 0x7FU));
    const uint8_t min = bcd2dec((uint8_t)(r[1] & 0x7FU));
    const uint8_t hour = bcd2dec((uint8_t)(r[2] & 0x3FU));
    const uint8_t day = bcd2dec((uint8_t)(r[3] & 0x3FU));
    const uint8_t weekday = (uint8_t)(r[4] & 0x07U);
    const uint8_t month = bcd2dec((uint8_t)(r[5] & 0x1FU));
    const uint8_t year2 = bcd2dec(r[6]);

    // Century bit (r[5] bit7): 0 = 20xx, 1 = 19xx per datasheet.
    // Most boards use 20xx.
    const bool century_19xx = (r[5] & 0x80U) != 0;
    const uint16_t year_full = (uint16_t)((century_19xx ? 1900U : 2000U) + year2);

    t->voltage_low = vl;
    t->second = sec;
    t->minute = min;
    t->hour = hour;
    t->day = day;
    t->weekday = weekday;
    t->month = month;
    t->year = year_full;

    return true;
}

bool PCF8563_SetTime(const pcf8563_time_t *t)
{
    if (!t) {
        return false;
    }

    // Basic validation
    if (t->month < 1 || t->month > 12 || t->day < 1 || t->day > 31 ||
        t->hour > 23 || t->minute > 59 || t->second > 59 || t->weekday > 6) {
        return false;
    }

    uint16_t year = t->year;
    bool century_19xx = false;
    if (year >= 2000 && year <= 2099) {
        century_19xx = false;
    } else if (year >= 1900 && year <= 1999) {
        century_19xx = true;
    } else {
        // Keep it simple: only support 19xx/20xx.
        return false;
    }

    const uint8_t year2 = (uint8_t)(year % 100U);

    uint8_t r[7];
    r[0] = (uint8_t)(dec2bcd(t->second) & 0x7FU);          // VL cleared
    r[1] = (uint8_t)(dec2bcd(t->minute) & 0x7FU);
    r[2] = (uint8_t)(dec2bcd(t->hour) & 0x3FU);
    r[3] = (uint8_t)(dec2bcd(t->day) & 0x3FU);
    r[4] = (uint8_t)(t->weekday & 0x07U);
    r[5] = (uint8_t)(dec2bcd(t->month) & 0x1FU);
    if (century_19xx) {
        r[5] |= 0x80U;
    }
    r[6] = dec2bcd(year2);

    return PCF8563_WriteRaw(0x02, r, (uint8_t)sizeof(r));
}
