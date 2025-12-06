#ifdef ENABLE_FLASHLIGHT

#include <Arduino.h>
#include "flashlight.h"

enum FlashlightMode_t  gFlashLightState;
static uint8_t flashlight_pin_state = LOW;

void FlashlightTimeSlice()
{
    if (gFlashLightState == FLASHLIGHT_BLINK && (gFlashLightBlinkCounter & 15u) == 0) {
        // Toggle flashlight state
        flashlight_pin_state = !flashlight_pin_state;
        digitalWrite(GPIOC_PIN_FLASHLIGHT, flashlight_pin_state);
        return;
    }

    if (gFlashLightState == FLASHLIGHT_SOS) {
        const uint16_t u = 15;
        static uint8_t c;
        static uint16_t next;

        if (gFlashLightBlinkCounter - next > 7 * u) {
            c = 0;
            next = gFlashLightBlinkCounter + 1;
            return;
        }

        if (gFlashLightBlinkCounter == next) {
            if (c==0) {
                digitalWrite(GPIOC_PIN_FLASHLIGHT, LOW);
                flashlight_pin_state = LOW;
            } else {
                // Toggle flashlight state
                flashlight_pin_state = !flashlight_pin_state;
                digitalWrite(GPIOC_PIN_FLASHLIGHT, flashlight_pin_state);
            }

            if (c >= 18) {
                next = gFlashLightBlinkCounter + 7 * u;
                c = 0;
            } else if(c==7 || c==9 || c==11) {
                next = gFlashLightBlinkCounter + 3 * u;
            } else {
                next = gFlashLightBlinkCounter + u;
            }
            c++;
        }
    }
}

void ACTION_FlashLight(void)
{
    // Initialize pin as output on first use
    static bool initialized = false;
    if (!initialized) {
        pinMode(GPIOC_PIN_FLASHLIGHT, OUTPUT);
        initialized = true;
    }

    if(gFlashLightState)
        {
        digitalWrite(GPIOC_PIN_FLASHLIGHT, LOW);
        flashlight_pin_state = LOW;
        gFlashLightState=0;

        }else
            {
        digitalWrite(GPIOC_PIN_FLASHLIGHT, HIGH);
        flashlight_pin_state = HIGH;
        gFlashLightState=1;
            }
}

#endif