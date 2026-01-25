#include "driver/es8311.h"

#include "driver/i2c1.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

namespace {
constexpr uint8_t kEs8311Address = 0x18; // 7-bit I2C address
constexpr uint8_t kEsResetMasterBit = 0x40;

constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr uint32_t kI2sSampleRate = 48000;
constexpr i2s_bits_per_sample_t kI2sBits = I2S_BITS_PER_SAMPLE_16BIT;
constexpr uint32_t kToneFrames = 256;
constexpr float kToneFrequency = 440.0f;
constexpr float kToneAmplitude = 0.40f;
constexpr float kTwoPi = 6.283185307179586f;

static bool s_es8311_ready = false;
static bool s_es8311_i2s_ready = false;
static bool s_i2s_driver_installed = false;

static void es8311_log(const char *format, ...) {
    static constexpr size_t kMaxLogLen = 200;
    char buffer[kMaxLogLen];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, kMaxLogLen, format, args);
    va_end(args);
    Serial.println(buffer);
}

enum {
    kRegReset = 0x00,
    kRegClkManager1 = 0x01,
    kRegClkManager2 = 0x02,
    kRegClkManager3 = 0x03,
    kRegClkManager4 = 0x04,
    kRegClkManager5 = 0x05,
    kRegClkManager6 = 0x06,
    kRegClkManager7 = 0x07,
    kRegClkManager8 = 0x08,
    kRegSystem0B = 0x0B,
    kRegSystem0C = 0x0C,
    kRegSystem10 = 0x10,
    kRegSystem11 = 0x11,
    kRegSystem13 = 0x13,
    kRegAdc16 = 0x16,
    kRegAdc1B = 0x1B,
    kRegAdc1C = 0x1C,
    kRegDac31 = 0x31,
    kRegDac32 = 0x32,
    kRegGpio44 = 0x44,
};

static bool es8311_write_register(uint8_t reg, uint8_t value) {
    bool ok = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        I2C_Start();
        ok = true;
        if (I2C_Write((kEs8311Address << 1) | I2C_WRITE) < 0) {
            ok = false;
            goto stop_once;
        }
        if (I2C_Write(reg) < 0) {
            ok = false;
            goto stop_once;
        }
        if (I2C_Write(value) < 0) {
            ok = false;
            goto stop_once;
        }
    stop_once:
        I2C_Stop();
        if (ok) {
            break;
        }
        delay(1);
    }
    es8311_log("[ES8311] write 0x%02X = 0x%02X -> %s", reg, value, ok ? "OK" : "FAIL");
    return ok;
}

static bool es8311_read_register(uint8_t reg, uint8_t *value) {
    bool ok = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        I2C_Start();
        ok = true;
        if (I2C_Write((kEs8311Address << 1) | I2C_WRITE) < 0) {
            ok = false;
            goto stop_once_read;
        }
        if (I2C_Write(reg) < 0) {
            ok = false;
            goto stop_once_read;
        }
        I2C_Start();
        if (I2C_Write((kEs8311Address << 1) | I2C_READ) < 0) {
            ok = false;
            goto stop_once_read;
        }
        *value = I2C_Read(true);
    stop_once_read:
        I2C_Stop();
        if (ok) {
            break;
        }
        delay(1);
    }
    es8311_log("[ES8311] read 0x%02X -> 0x%02X (%s)", reg, *value, ok ? "OK" : "FAIL");
    return ok;
}

static bool ensure_i2s_ready(void) {
    if (s_es8311_i2s_ready) {
        return true;
    }

    if (s_i2s_driver_installed) {
        i2s_driver_uninstall(kI2sPort);
        s_i2s_driver_installed = false;
    }

    i2s_config_t config = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = static_cast<int>(kI2sSampleRate),
        .bits_per_sample = kI2sBits,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 4,
        .dma_buf_len = 128,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };

    if (i2s_driver_install(kI2sPort, &config, 0, nullptr) != ESP_OK) {
        return false;
    }

    s_i2s_driver_installed = true;

    i2s_pin_config_t pins = {
        .mck_io_num = 35,
        .bck_io_num = 42,
        .ws_io_num = 37,
        .data_out_num = 36,
        .data_in_num = 38,
    };

    if (i2s_set_pin(kI2sPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kI2sPort);
        return false;
    }

    if (i2s_set_clk(kI2sPort, kI2sSampleRate, kI2sBits, I2S_CHANNEL_STEREO) != ESP_OK) {
        i2s_driver_uninstall(kI2sPort);
        return false;
    }

    s_es8311_i2s_ready = true;
    return true;
}

} // namespace

bool ES8311_Init(void) {
    if (s_es8311_ready) {
        return true;
    }

    I2C_Init();

    bool ok = true;
    ok &= es8311_write_register(kRegGpio44, 0x08);
    ok &= es8311_write_register(kRegGpio44, 0x08);

    ok &= es8311_write_register(kRegClkManager1, 0x30);
    ok &= es8311_write_register(kRegClkManager2, 0x00);
    ok &= es8311_write_register(kRegClkManager3, 0x10);
    ok &= es8311_write_register(kRegAdc16, 0x24);
    ok &= es8311_write_register(kRegClkManager4, 0x10);
    ok &= es8311_write_register(kRegClkManager5, 0x00);
    ok &= es8311_write_register(kRegSystem0B, 0x00);
    ok &= es8311_write_register(kRegSystem0C, 0x00);
    ok &= es8311_write_register(kRegSystem10, 0x1F);
    ok &= es8311_write_register(kRegSystem11, 0x7F);
    ok &= es8311_write_register(kRegReset, 0x80);

    uint8_t reset_value = 0;
    ok &= es8311_read_register(kRegReset, &reset_value);
    reset_value &= ~kEsResetMasterBit; // ensure slave mode (ESP32 drives BCLK)
    ok &= es8311_write_register(kRegReset, reset_value);

    ok &= es8311_write_register(kRegClkManager1, 0x3F);

    ok &= es8311_write_register(kRegClkManager2, 0x00);
    ok &= es8311_write_register(kRegClkManager5, 0x00);
    ok &= es8311_write_register(kRegClkManager3, 0x10);
    ok &= es8311_write_register(kRegClkManager4, 0x10);
    ok &= es8311_write_register(kRegClkManager7, 0x00);
    ok &= es8311_write_register(kRegClkManager8, 0x04);
    ok &= es8311_write_register(kRegClkManager6, 0x0F);

    ok &= es8311_write_register(kRegSystem13, 0x10);
    ok &= es8311_write_register(kRegAdc1B, 0x0A);
    ok &= es8311_write_register(kRegAdc1C, 0x6A);
    ok &= es8311_write_register(kRegDac31, 0x00);
    ok &= es8311_write_register(kRegDac32, 0x7F);

    s_es8311_ready = ok;
    return ok;
}

bool ES8311_IsReady(void) {
    return s_es8311_ready;
}

bool ES8311_PlayTestTone(uint32_t durationMs) {
    if (!ES8311_Init()) {
        return false;
    }

    if (!ensure_i2s_ready()) {
        return false;
    }

    static int16_t tone_buffer[kToneFrames * 2];
    for (size_t i = 0; i < kToneFrames; ++i) {
        const float angle = kTwoPi * kToneFrequency * static_cast<float>(i) / static_cast<float>(kI2sSampleRate);
        const int16_t sample = static_cast<int16_t>(sinf(angle) * kToneAmplitude * static_cast<float>(INT16_MAX));
        tone_buffer[i * 2 + 0] = sample;
        tone_buffer[i * 2 + 1] = sample;
    }

    const size_t buffer_bytes = sizeof(tone_buffer);
    uint64_t loops = (static_cast<uint64_t>(durationMs) * kI2sSampleRate) / (kToneFrames * 1000);
    if (loops == 0) {
        loops = 1;
    }

    for (uint64_t loop = 0; loop < loops; ++loop) {
        size_t written = 0;
        const esp_err_t err = i2s_write(kI2sPort, tone_buffer, buffer_bytes, &written, portMAX_DELAY);
        if (err != ESP_OK || written != buffer_bytes) {
            return false;
        }
    }

    return true;
}
