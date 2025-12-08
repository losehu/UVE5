#include <Arduino.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "lib/shared_flash.h"



#include "driver/gpio.h"
#include "app/si.h"
#include "app/scheduler.h"
#include "driver/i2c.h"
#include "driver/system.h"
#include "frequencies.h"
#include "misc.h"
#include "app/doppler.h"
#include "driver/uart.h"
#include "string.h"
#include <stdio.h>
#include "ui/helper.h"
#include <string.h>
#include "driver/bk4819.h"
#include "font.h"
#include "ui/ui.h"
#include <stdint.h>
#include <string.h>
#include "font.h"
#include <stdio.h>     // NULL
#include "app/mdc1200.h"
#include "app/uart.h"
#include "string.h"
#include "app/messenger.h"
#include "driver/adc1.h"
#ifdef ENABLE_DOPPLER

#include "app/doppler.h"

#endif
#ifdef ENABLE_AM_FIX

#include "am_fix.h"

#endif


#ifdef ENABLE_TIMER
#include "bsp/dp32g030/uart.h"
#include "bsp/dp32g030/timer.h"
#endif
#ifdef ENABLE_4732


#endif

#include "audio.h"
#include "board.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "version.h"
#include "app/app.h"
#include "app/dtmf.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "driver/systick.h"

#ifdef ENABLE_UART

#include "driver/uart.h"

#endif

#include "app/spectrum.h"

#include "helper/battery.h"
#include "helper/boot.h"

#include "ui/lock.h"
#include "ui/welcome.h"
#include "ui/menu.h"
#include "driver/eeprom.h"
#include "driver/st7565.h"








void switch_to_factory_and_restart() {
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
  if (part) {
    esp_ota_set_boot_partition(part);
    // esp_restart();
  } else {
    // 未找到 factory 分区
  }
}

 
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
#ifdef ENABLE_DOPPLER

    RTC_INIT();
    INIT_DOPPLER_DATA();
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
     UI_DisplayWelcome();
#ifdef ENABLE_BOOTLOADER
    if(KEYBOARD_Poll() == KEY_MENU)
{
            for (int i = 0; i < 10*1024; i += 4) {
                uint32_t c;
                EEPROM_ReadBuffer(0x41000 + i, (uint8_t *) &c, 4);
                write_to_memory(0x20001000 + i, c);
            }
            JUMP_TO_FLASH(0x2000110a, 0x20003ff0);
}
#endif

    boot_counter_10ms = 250;

    while (boot_counter_10ms > 0 || (KEYBOARD_Poll() != KEY_INVALID)) {

        if (KEYBOARD_Poll() == KEY_EXIT
#if ENABLE_CHINESE_FULL == 4
            || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_NONE
#endif
                ) {    // halt boot beeps
            boot_counter_10ms = 0;
            break;
        }

    }




    //	BOOT_ProcessMode();
    GUI_SelectNextDisplay(DISPLAY_MAIN);
    gpio_set_level(GPIOA_PIN_VOICE_0 , 0);

    gUpdateStatus = true;

#ifdef ENABLE_VOICE
    {
        uint8_t Channel;

        AUDIO_SetVoiceID(0, VOICE_ID_WELCOME);

        Channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];
        if (IS_MR_CHANNEL(Channel))
        {
            AUDIO_SetVoiceID(1, VOICE_ID_CHANNEL_MODE);
            AUDIO_SetDigitVoice(2, Channel + 1);
        }
        else if (IS_FREQ_CHANNEL(Channel))
            AUDIO_SetVoiceID(1, VOICE_ID_FREQUENCY_MODE);

        AUDIO_PlaySingleVoice(0);
    }
#endif

#ifdef ENABLE_NOAA
    RADIO_ConfigureNOAA();
#endif

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
