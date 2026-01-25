#ifndef DRIVER_ES8311_H
#define DRIVER_ES8311_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the ES8311 codec (I2C configuration + power-up).
bool ES8311_Init(void);

// Returns true when the test tone was accepted and scheduled.
bool ES8311_PlayTestTone(uint32_t durationMs);

// True if the codec initialization sequence already succeeded.
bool ES8311_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif // DRIVER_ES8311_H
