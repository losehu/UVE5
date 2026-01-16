#ifndef ENABLE_OPENCV
#include <Arduino.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#else
#include "opencv/Arduino.hpp"
#include <cstdlib>
#endif
#include "../lib/shared_flash.h"
#include "app/si.h"
#include "scheduler.h"
#include "driver/system.h"
#include "frequencies.h"
#include "misc.h"
#include "app/doppler.h"
#include "string.h"
#include <stdio.h>
#include "ui/ui.h"
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
#ifdef ENABLE_MESSENGER
#include "app/messenger.h"  
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
#include "driver/keyboard.h"

#if !defined(ENABLE_OPENCV)
#include "assets/ckx_8lvl_128x64.h"
#endif



#if defined(ENABLE_ARDUBOY_AVR) && (ENABLE_ARDUBOY_AVR)
#include "app/arduboy_avr.h"
#endif
#ifndef ENABLE_SCREEN_FPS_TEST
#define ENABLE_SCREEN_FPS_TEST 1
#endif
#ifndef ENABLE_SCREEN_FPS_TEST
#define ENABLE_SCREEN_FPS_TEST 1
#endif

#ifndef SCREEN_TEST_GRAY_LEVELS
#define SCREEN_TEST_GRAY_LEVELS 8
#endif

#ifndef SCREEN_TEST_BANDS
#define SCREEN_TEST_BANDS 8
#endif

#ifndef SCREEN_TEST_TARGET_FPS
#define SCREEN_TEST_TARGET_FPS 740     // blit频率：保持你现在的高速刷新能力 886 1056
#endif

#if ENABLE_SCREEN_FPS_TEST
static void ScreenFpsTest() {
    Serial.println("[GRAY_NO_GRAIN_TEST] 8阶灰度(无颗粒) / UP-DOWN调fps / EXIT退出");

    static constexpr uint32_t kLevels = SCREEN_TEST_GRAY_LEVELS;
    uint32_t targetFps = SCREEN_TEST_TARGET_FPS;

#if SCREEN_TEST_GRAY_LEVELS == 4
    // 4阶：用 24 相位可得到精确 1/3 占空比步进（0,8,16,24）
    static constexpr uint32_t kPhases = 24;
#elif SCREEN_TEST_GRAY_LEVELS == 8
    // 8阶：用 56 相位可得到精确 1/7 占空比步进（0,8,16,...,56）
    static constexpr uint32_t kPhases = 24;
#else
    // 其它级数：通用相位数（越大越细腻，但对目标 FPS 要求更高）
    static constexpr uint32_t kPhases = 64;
#endif

    // Gradient per column (each band is one gray level)
    uint8_t target_lvl[LCD_WIDTH];
    for (uint32_t x = 0; x < LCD_WIDTH; ++x) {
        const uint32_t bands = (SCREEN_TEST_BANDS < 2) ? 2U : (uint32_t)SCREEN_TEST_BANDS;
        const uint32_t band  = (x * bands) / LCD_WIDTH;
        const uint32_t lvl   = (band * (kLevels - 1U)) / (bands - 1U);
        target_lvl[x] = (uint8_t)lvl;
    }

    // Per-column error diffusion accumulator (fixed phase offset => desync flicker, no grain)
    static uint16_t acc[LCD_WIDTH];
    static bool acc_inited = false;
    if (!acc_inited) {
        for (uint32_t x = 0; x < LCD_WIDTH; ++x) {
            acc[x] = (uint16_t)((x * 7u + 3u) % kPhases);
        }
        acc_inited = true;
    }

    uint32_t start_ms = millis();
    uint32_t last_report_ms = start_ms;
    uint32_t frames = 0;
    uint64_t blit_us_sum = 0;
    uint32_t blit_us_min = 0xFFFFFFFFu;
    uint32_t blit_us_max = 0;

    // Serial.printf("[GRAY_NO_GRAIN_TEST] levels=%lu bands=%u target_fps=%lu phases=%lu\n",
    //               (unsigned long)kLevels,
    //               (unsigned)SCREEN_TEST_BANDS,
    //               (unsigned long)targetFps,
    //               (unsigned long)kPhases);

    // Gray mapping profile (compensate LCD contrast nonlinearity)
    // KEY_MENU cycles profiles.
    // Note: we print profiles as 1..3 (internally 0..2).
    uint8_t dutyProfile = 2;
    // Serial.printf("[GRAY_NO_GRAIN_TEST] duty_profile=%u/3 (MENU to cycle)\n", (unsigned)(dutyProfile + 1u));

    uint32_t frame_idx = 0;
    uint32_t lastAdjustMs = 0;
    KEY_Code_t lastKey = KEY_INVALID;

    // Frame pacing: use a phase-locked scheduler to reduce jitter.
    uint32_t next_frame_us = micros();
    uint32_t work_us_ema = 0;
    bool work_ema_inited = false;
    uint32_t overrun_count = 0;
    uint32_t last_effective_fps = 0;

    while (true) {
        const uint32_t frame_start_us = micros();

        // Build one 1-bit frame using pure temporal PWM (no spatial dithering => no grain).
        uint8_t lineBytes[LCD_WIDTH];
        for (uint32_t x = 0; x < LCD_WIDTH; ++x) {
            const uint32_t lvl = target_lvl[x];

            // Map lvl -> duty (0..kPhases), using a perceptual-ish curve.
            // This helps when early grays look too white and late grays saturate to black.
            uint32_t duty = 0;
            if (kLevels >= 2) {
#if SCREEN_TEST_GRAY_LEVELS == 8
                // Percent of kPhases for each level.
                // profile 0: near-linear
                // profile 1: boost highlights + compress shadows (default)
                // profile 2: more aggressive highlight boost
                static constexpr uint8_t pct0[8] = {0, 14, 29, 43, 57, 71, 86, 100};
                static constexpr uint8_t pct1[8] = {0, 22, 36, 50, 64, 79, 90, 100};
                static constexpr uint8_t pct2[8] = {0, 33, 42, 50, 57, 65, 80, 100};
                // static constexpr uint8_t pct2[8] = {0, 13, 25, 38, 50, 63, 88, 100};

                const uint8_t i = (uint8_t)(lvl & 7u);
                const uint8_t p = (dutyProfile == 0) ? pct0[i] : (dutyProfile == 2) ? pct2[i] : pct1[i];
                duty = (uint32_t)((p * kPhases + 50u) / 100u);
#else
                // Generic linear mapping.
                duty = (lvl * kPhases) / (kLevels - 1U);
#endif

                // Ensure endpoints are exact.
                if (lvl == 0) {
                    duty = 0;
                } else if (lvl >= (kLevels - 1U)) {
                    duty = kPhases;
                }
            }

            uint32_t a = (uint32_t)acc[x] + duty;
            const bool on = (a >= kPhases);
            if (on) a -= kPhases;
            acc[x] = (uint16_t)a;

            // 极性：on->0xFF(黑) off->0x00(白)
            lineBytes[x] = on ? 0xFF : 0x00;
        }

        memcpy(gStatusLine, lineBytes, LCD_WIDTH);
        for (uint32_t p = 0; p < FRAME_LINES; ++p) {
            memcpy(gFrameBuffer[p], lineBytes, LCD_WIDTH);
        }

        const uint32_t t0 = micros();
        ST7565_BlitAll();
        const uint32_t t1 = micros();
        const uint32_t dt = (uint32_t)(t1 - t0);

        blit_us_sum += dt;
        if (dt < blit_us_min) blit_us_min = dt;
        if (dt > blit_us_max) blit_us_max = dt;
        ++frames;
        ++frame_idx;

        const uint32_t now_ms = millis();
        if ((uint32_t)(now_ms - last_report_ms) >= 1000U) {
            const uint32_t elapsed_ms = (uint32_t)(now_ms - start_ms);
            const float fps = elapsed_ms ? (frames * 1000.0f / elapsed_ms) : 0.0f;
            const uint32_t avg_us = frames ? (uint32_t)(blit_us_sum / frames) : 0;
            const float blit_fps_theoretical = avg_us ? (1000000.0f / avg_us) : 0.0f;

            Serial.printf("[GRAY_NO_GRAIN_TEST] fps=%.2f frames=%lu avg_us=%u min_us=%u max_us=%u\n",
                          fps, (unsigned long)frames, avg_us, blit_us_min, blit_us_max);
            Serial.printf("[GRAY_NO_GRAIN_TEST] blit_fps_theoretical=%.2f (1e6/avg_us)\n", blit_fps_theoretical);

            if (work_ema_inited && work_us_ema) {
                const uint32_t min_period_us = (uint32_t)((work_us_ema * 115U) / 100U); // ~15% headroom
                const uint32_t safe_fps = min_period_us ? (1000000U / min_period_us) : 0U;
                                Serial.printf("[GRAY_NO_GRAIN_TEST] work_us_ema=%lu safe_fps~%lu overruns=%lu\n",
                                                            (unsigned long)work_us_ema,
                                                            (unsigned long)safe_fps,
                                                            (unsigned long)overrun_count);
            }

            last_report_ms = now_ms;
        }

        // Poll key once per loop: EXIT + runtime FPS tune
        const KEY_Code_t key = KEYBOARD_Poll();
        if (key == KEY_EXIT) break;

        const uint32_t now_ms2 = millis();
        const bool isEdge = (key != KEY_INVALID) && (key != lastKey);
        const bool isRepeat = (key != KEY_INVALID) && (key == lastKey);
        const uint32_t repeatDelayMs = 80U;

        if (key == KEY_MENU && isEdge) {
            dutyProfile = (uint8_t)((dutyProfile + 1u) % 3u);
            Serial.printf("[GRAY_NO_GRAIN_TEST] duty_profile=%u/3\n", (unsigned)(dutyProfile + 1u));
            lastAdjustMs = now_ms2;
        }

        if ((key == KEY_UP || key == KEY_DOWN) && (isEdge || ((uint32_t)(now_ms2 - lastAdjustMs) >= repeatDelayMs && isRepeat))) {
            const uint32_t step = 20U;
            const uint32_t minFps = 100U;
            const uint32_t maxFps = 1500U;

            if (key == KEY_UP) {
                targetFps = (targetFps + step <= maxFps) ? (targetFps + step) : maxFps;
            } else {
                targetFps = (targetFps > minFps + step) ? (targetFps - step) : minFps;
            }
            Serial.printf("[GRAY_NO_GRAIN_TEST] target_fps=%lu\n", (unsigned long)targetFps);
            lastAdjustMs = now_ms2;
            // Reset scheduler phase when changing target FPS.
            next_frame_us = micros();
        }
        lastKey = key;

        // Estimate work time (everything except the deliberate wait).
        const uint32_t work_us = (uint32_t)(micros() - frame_start_us);
        if (!work_ema_inited) {
            work_us_ema = work_us;
            work_ema_inited = true;
        } else {
            // EMA with alpha=1/8
            work_us_ema = (uint32_t)((work_us_ema * 7U + work_us) / 8U);
        }

        // No auto-limiting: user target is the target.
        // If the loop can't keep up, we'll just overrun and report it.
        const uint32_t framePeriodUs = (targetFps > 0) ? (1000000U / targetFps) : 0U;

        // Phase-locked frame pacing: schedule the next frame boundary.
        // This reduces cadence jitter vs "work + delay" style loops.
        if (framePeriodUs) {
            next_frame_us += framePeriodUs;
            const int32_t remaining = (int32_t)(next_frame_us - micros());
            if (remaining > 0) {
                // Sleep most of the time, then spin a little for precision.
                const uint32_t rem_u = (uint32_t)remaining;
                if (rem_u > 20U) {
                    delayMicroseconds(rem_u - 10U);
                }
                while ((int32_t)(next_frame_us - micros()) > 0) {
                    // tight wait
                }
            } else {
                // Overrun: we missed the deadline.
                ++overrun_count;
                next_frame_us = micros();
            }
        }
    }

    memset(gStatusLine, 0x00, sizeof(gStatusLine));
    for (unsigned page = 0; page < FRAME_LINES; ++page) {
        memset(gFrameBuffer[page], 0x00, LCD_WIDTH);
    }
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
    Serial.println("[GRAY_NO_GRAIN_TEST] Done");
}
#endif


#ifndef ENABLE_SCREEN_CKX_TEST
#define ENABLE_SCREEN_CKX_TEST 1
#endif

#if ENABLE_SCREEN_CKX_TEST && !defined(ENABLE_OPENCV)
static void ScreenCkxTest() {
    // Serial.println("[CKX_TEST] ckx.jpeg -> 8级灰度(时间PWM) / UP-DOWN调fps / MENU切曲线 / EXIT退出");

    static constexpr uint32_t kLevels = 8;
    static constexpr uint32_t kPhases = 56; // 8阶推荐：精确 1/7 占空比步进

    uint32_t targetFps = SCREEN_TEST_TARGET_FPS;

    // Hand-tuned level->percent mapping profiles.
    // NOTE: you can tweak pct2 here for your LCD.
    static constexpr uint8_t pct0[8] = {0, 14, 29, 43, 57, 71, 86, 100};
    static constexpr uint8_t pct1[8] = {0, 22, 36, 50, 64, 79, 90, 100};
    static constexpr uint8_t pct2[8] = {0, 13, 25, 38, 50, 63, 88, 100};

    uint8_t dutyProfile = 2;
    Serial.printf("[CKX_TEST] target_fps=%lu phases=%lu duty_profile=%u/3\n",
                  (unsigned long)targetFps,
                  (unsigned long)kPhases,
                  (unsigned)(dutyProfile + 1u));

    // Per-pixel error diffusion accumulator (no spatial dithering).
    static uint16_t acc[LCD_WIDTH * 64];
    static bool acc_inited = false;
    if (!acc_inited) {
        for (uint32_t y = 0; y < 64; ++y) {
            for (uint32_t x = 0; x < LCD_WIDTH; ++x) {
                const uint32_t idx = y * LCD_WIDTH + x;
                acc[idx] = (uint16_t)((x * 7u + y * 11u + 3u) % kPhases);
            }
        }
        acc_inited = true;
    }

    uint32_t lastAdjustMs = 0;
    KEY_Code_t lastKey = KEY_INVALID;

    // Locked frame pacing.
    uint32_t next_frame_us = micros();
    uint32_t overrun_count = 0;
    uint32_t start_ms = millis();
    uint32_t last_report_ms = start_ms;
    uint32_t frames = 0;
    uint64_t blit_us_sum = 0;
    uint32_t blit_us_min = 0xFFFFFFFFu;
    uint32_t blit_us_max = 0;

    while (true) {
        // Clear buffers
        memset(gStatusLine, 0x00, sizeof(gStatusLine));
        for (uint32_t page = 0; page < FRAME_LINES; ++page) {
            memset(gFrameBuffer[page], 0x00, LCD_WIDTH);
        }

        // Build 1-bit frame from 8-level image using temporal PWM.
        for (uint32_t y = 0; y < 64; ++y) {
            const uint32_t page = (y < 8) ? 0U : ((y - 8) / 8U);
            const uint8_t bit = (uint8_t)(1u << ((y < 8) ? y : ((y - 8) & 7u)));
            for (uint32_t x = 0; x < LCD_WIDTH; ++x) {
                const uint32_t idx = y * LCD_WIDTH + x;
                const uint8_t lvl = (uint8_t)(CKX_8LVL_128x64[idx] & 7u);

                const uint8_t p = (dutyProfile == 0) ? pct0[lvl] : (dutyProfile == 2) ? pct2[lvl] : pct1[lvl];
                uint32_t duty = (uint32_t)((p * kPhases + 50u) / 100u);

                // Ensure endpoints are exact.
                if (lvl == 0) duty = 0;
                else if (lvl >= (kLevels - 1U)) duty = kPhases;

                uint32_t a = (uint32_t)acc[idx] + duty;
                const bool on = (a >= kPhases);
                if (on) a -= kPhases;
                acc[idx] = (uint16_t)a;

                if (on) {
                    if (y < 8) {
                        gStatusLine[x] |= bit;
                    } else {
                        gFrameBuffer[page][x] |= bit;
                    }
                }
            }
        }

        const uint32_t t0 = micros();
        ST7565_BlitAll();
        const uint32_t t1 = micros();
        const uint32_t dt = (uint32_t)(t1 - t0);
        blit_us_sum += dt;
        if (dt < blit_us_min) blit_us_min = dt;
        if (dt > blit_us_max) blit_us_max = dt;
        ++frames;

        const uint32_t now_ms = millis();
        if ((uint32_t)(now_ms - last_report_ms) >= 1000U) {
            const uint32_t elapsed_ms = (uint32_t)(now_ms - start_ms);
            const float fps = elapsed_ms ? (frames * 1000.0f / elapsed_ms) : 0.0f;
            const uint32_t avg_us = frames ? (uint32_t)(blit_us_sum / frames) : 0;
            Serial.printf("[CKX_TEST] fps=%.2f target=%lu avg_us=%u min_us=%u max_us=%u overruns=%lu\n",
                          fps,
                          (unsigned long)targetFps,
                          avg_us,
                          blit_us_min,
                          blit_us_max,
                          (unsigned long)overrun_count);
            last_report_ms = now_ms;
        }

        // Input
        const KEY_Code_t key = KEYBOARD_Poll();
        if (key == KEY_EXIT) break;

        const uint32_t now_ms2 = millis();
        const bool isEdge = (key != KEY_INVALID) && (key != lastKey);
        const bool isRepeat = (key != KEY_INVALID) && (key == lastKey);
        const uint32_t repeatDelayMs = 80U;

        if (key == KEY_MENU && isEdge) {
            dutyProfile = (uint8_t)((dutyProfile + 1u) % 3u);
            Serial.printf("[CKX_TEST] duty_profile=%u/3\n", (unsigned)(dutyProfile + 1u));
            lastAdjustMs = now_ms2;
        }

        if ((key == KEY_UP || key == KEY_DOWN) && (isEdge || ((uint32_t)(now_ms2 - lastAdjustMs) >= repeatDelayMs && isRepeat))) {
            const uint32_t step = 20U;
            const uint32_t minFps = 50U;
            const uint32_t maxFps = 2000U;

            if (key == KEY_UP) {
                targetFps = (targetFps + step <= maxFps) ? (targetFps + step) : maxFps;
            } else {
                targetFps = (targetFps > minFps + step) ? (targetFps - step) : minFps;
            }
            Serial.printf("[CKX_TEST] target_fps=%lu\n", (unsigned long)targetFps);
            lastAdjustMs = now_ms2;
            next_frame_us = micros();
        }
        lastKey = key;

        // Locked pacing
        const uint32_t framePeriodUs = (targetFps > 0) ? (1000000U / targetFps) : 0U;
        if (framePeriodUs) {
            next_frame_us += framePeriodUs;
            const int32_t remaining = (int32_t)(next_frame_us - micros());
            if (remaining > 0) {
                const uint32_t rem_u = (uint32_t)remaining;
                if (rem_u > 20U) {
                    delayMicroseconds(rem_u - 10U);
                }
                while ((int32_t)(next_frame_us - micros()) > 0) {
                }
            } else {
                ++overrun_count;
                next_frame_us = micros();
            }
        }
    }

    Serial.println("[CKX_TEST] Done");
}
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

#if ENABLE_SCREEN_FPS_TEST
    // Auto-run LCD FPS test on boot.
#if ENABLE_SCREEN_CKX_TEST && !defined(ENABLE_OPENCV)
    ScreenCkxTest();
#else
    ScreenFpsTest();
#endif
#endif


}



void loop() {
    Serial.println("Starting main application...");
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

#if defined(ENABLE_OPENCV) && defined(ENABLE_ARDUBOY_AVR) && (ENABLE_ARDUBOY_AVR)
    {
        static bool did_autorun = false;
        const char *env = std::getenv("UVE5_ARDUBOY_AVR_AUTORUN");
        if (!did_autorun && env && env[0] != '\0' && env[0] != '0') {
            did_autorun = true;
            ARDUBOY_AVR_Enter();
        }
    }
#endif

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
