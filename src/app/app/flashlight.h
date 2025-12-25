#ifndef APP_FLASHLIGHT_H
#define APP_FLASHLIGHT_H

#ifdef ENABLE_FLASHLIGHT

#include <stdint.h>

enum FlashlightMode_t {
    FLASHLIGHT_OFF = 0,
    FLASHLIGHT_ON,
    FLASHLIGHT_BLINK,
    FLASHLIGHT_SOS
};

extern enum FlashlightMode_t gFlashLightState;
extern volatile uint32_t     gFlashLightBlinkCounter;

void FlashlightTimeSlice(void);
void ACTION_FlashLight(void);
#define GPIOC_PIN_FLASHLIGHT ((gpio_num_t)33)  // 手电筒控制引脚

#endif

#endif