#ifndef DRIVER_PCF8563_H
#define DRIVER_PCF8563_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// PCF8563 7-bit I2C address
#define PCF8563_I2C_ADDR_7BIT 0x51

typedef struct {
    // Full year, e.g. 2026
    uint16_t year;
    uint8_t month;    // 1..12
    uint8_t day;      // 1..31
    uint8_t weekday;  // 0..6
    uint8_t hour;     // 0..23
    uint8_t minute;   // 0..59
    uint8_t second;   // 0..59
    bool voltage_low; // seconds register VL bit
} pcf8563_time_t;

bool PCF8563_Probe(void);
bool PCF8563_ReadRaw(uint8_t reg, void *buf, uint8_t len);
bool PCF8563_WriteRaw(uint8_t reg, const void *buf, uint8_t len);

bool PCF8563_ReadTime(pcf8563_time_t *t);

// Writes time back to RTC (seconds..years registers). Returns false on I2C failure or invalid fields.
bool PCF8563_SetTime(const pcf8563_time_t *t);

#ifdef __cplusplus
}
#endif

#endif // DRIVER_PCF8563_H
