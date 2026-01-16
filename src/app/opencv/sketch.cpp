#include "Arduino.hpp"

#include <cstdlib>
#include <cstring>

#include "app/app.h"
#include "app/dtmf.h"
#include "app/mdc1200.h"
#include "app/messenger.h"
#include "board.h"
#include "driver/adc1.h"
#include "driver/bk4819.h"
#include "driver/keyboard.h"
#include "driver/system.h"
#include "frequencies.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "scheduler.h"
#include "settings.h"
#include "ui/ui.h"
#include "ui/welcome.h"

#ifdef ENABLE_AM_FIX
#include "am_fix.h"
#endif

void setup() {
    switch_to_factory_and_restart();

    Serial.begin(115200);
    Serial.println("Initializing devices...");

    BOARD_Init();
    SCHEDULER_Init();

    memset(gDTMF_String, '-', sizeof(gDTMF_String));
    gDTMF_String[sizeof(gDTMF_String) - 1] = 0;

    BOARD_ADC_GetBatteryInfo(&gBatteryCurrentVoltage, &gBatteryCurrent);

    SETTINGS_InitEEPROM();
    SETTINGS_LoadCalibration();

#ifdef ENABLE_MESSENGER
    MSG_Init();
#endif
#ifdef ENABLE_MDC1200
    MDC1200_init();
#endif

    RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
    RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);

    RADIO_SelectVfos();
    RADIO_SetupRegisters(true);

    for (uint32_t i = 0; i < ARRAY_SIZE(gBatteryVoltages); i++) {
        BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[i], &gBatteryCurrent);
    }
    BATTERY_GetReadings(false);

#ifdef ENABLE_AM_FIX
    AM_fix_init();
#endif

#if ENABLE_CHINESE_FULL == 0
    gMenuListCount = 52;
#else
    gMenuListCount = 53;
#endif

    gKeyReading0 = KEY_INVALID;
    gKeyReading1 = KEY_INVALID;
    gDebounceCounter = 0;
}

void loop() {
    Serial.println("Starting main application...");

    UI_DisplayWelcome();

    boot_counter_10ms = 250;

    while (boot_counter_10ms > 0 || (KEYBOARD_Poll() != KEY_INVALID)) {
        if (KEYBOARD_Poll() == KEY_EXIT
#if ENABLE_CHINESE_FULL == 4
            || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE
#endif
        ) {
            boot_counter_10ms = 0;
            break;
        }
    }

    GUI_SelectNextDisplay(DISPLAY_MAIN);
    gpio_set_level(GPIOA_PIN_VOICE_0, 0);

    gUpdateStatus = true;

    while (1) {
        APP_Update();

        if (gNextTimeslice) {
            APP_TimeSlice10ms();
        }

        if (gNextTimeslice_500ms) {
            APP_TimeSlice500ms();
        }
    }
}
