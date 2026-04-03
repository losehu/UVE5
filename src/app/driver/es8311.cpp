#include "driver/es8311.h"

#include "driver/i2c1.h"

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef ENABLE_ES8311_TX_DTMF_TEST
#define ENABLE_ES8311_TX_DTMF_TEST 0
#endif

namespace {

constexpr uint8_t kEs8311Addr = 0x18; // 7-bit I2C address

constexpr int kPinDin = 36;   // codec DIN, data from MCU
constexpr int kPinDout = 38;  // codec DOUT, data to MCU
constexpr int kPinBclk = 42;
constexpr int kPinLrclk = 37;
constexpr int kPinMclk = 35;
constexpr int kPinAudioSwitch = 41;

constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr int kSampleRate = 8000;
constexpr i2s_bits_per_sample_t kBitsPerSample = I2S_BITS_PER_SAMPLE_16BIT;
constexpr size_t kFrameSamples = 80;
constexpr size_t kFrameBytes = kFrameSamples * sizeof(int16_t);
const TickType_t kI2sWaitTicks = pdMS_TO_TICKS(20);

constexpr float kToneFrequency = 440.0f;
constexpr float kToneAmplitude = 0.40f;
constexpr float kTwoPi = 6.283185307179586f;

#if ENABLE_ES8311_TX_DTMF_TEST
constexpr size_t kDtmfToneSamples = (kSampleRate * 180u) / 1000u;
constexpr size_t kDtmfGapSamples = (kSampleRate * 60u) / 1000u;
constexpr float kDtmfAmplitude = 0.24f;
#endif

// ES8311 registers (subset)
enum : uint8_t {
    ES8311_REG00_RESET = 0x00,
    ES8311_REG01_CLK_MANAGER = 0x01,
    ES8311_REG02_CLK_MANAGER = 0x02,
    ES8311_REG03_CLK_MANAGER = 0x03,
    ES8311_REG04_CLK_MANAGER = 0x04,
    ES8311_REG05_CLK_MANAGER = 0x05,
    ES8311_REG06_CLK_MANAGER = 0x06,
    ES8311_REG07_CLK_MANAGER = 0x07,
    ES8311_REG08_CLK_MANAGER = 0x08,
    ES8311_REG09_SDPIN = 0x09,
    ES8311_REG0A_SDPOUT = 0x0A,
    ES8311_REG0D_SYSTEM = 0x0D,
    ES8311_REG0E_SYSTEM = 0x0E,
    ES8311_REG12_SYSTEM = 0x12,
    ES8311_REG13_SYSTEM = 0x13,
    ES8311_REG14_SYSTEM = 0x14,
    ES8311_REG16_ADC = 0x16,
    ES8311_REG17_ADC = 0x17,
    ES8311_REG1C_ADC = 0x1C,
    ES8311_REG32_DAC = 0x32,
    ES8311_REG37_DAC = 0x37,
    ES8311_REG44_GPIO = 0x44,
};

constexpr uint8_t kEs8311ResetAll = 0x1F;
constexpr uint8_t kEs8311ResetRelease = 0x00;
constexpr uint8_t kEs8311PowerOn = 0x80;
constexpr uint8_t kEs8311ClockEnableAll = 0x3F;
constexpr uint8_t kEs8311AnalogMicPgaEnable = 0x1A; // mic gain from test.cpp
constexpr uint8_t kEs8311AdcGainScaleUp = 0x24;
constexpr uint8_t kEs8311AdcVolumeDefault = 0xE0;
constexpr uint8_t kEs8311DacVolumeDefault = 0x7F;
constexpr uint8_t kEs8311PowerUpAnalog = 0x01;
constexpr uint8_t kEs8311PowerUpPgaAdc = 0x02;
constexpr uint8_t kEs8311PowerUpDac = 0x00;
constexpr uint8_t kEs8311HpDriveEnable = 0x10;
constexpr uint8_t kEs8311AdcEqBypass = 0x6A;
constexpr uint8_t kEs8311DacEqBypass = 0x08;

struct Es8311ClockConfig {
    uint8_t pre_div;
    uint8_t pre_mult;
    uint8_t adc_div;
    uint8_t dac_div;
    uint8_t fs_mode;
    uint8_t lrck_h;
    uint8_t lrck_l;
    uint8_t bclk_div;
    uint8_t adc_osr;
    uint8_t dac_osr;
};

static bool s_es8311_ready = false;
static bool s_i2s_ready = false;
static bool s_i2s_driver_installed = false;
static TaskHandle_t s_passthrough_task = nullptr;
static volatile bool s_passthrough_running = false;
static ES8311_AudioMode_t s_audio_mode = ES8311_AUDIO_MODE_RECEIVE;
static ES8311_FrameHook_t s_frame_hook = nullptr;
static void *s_frame_hook_user_data = nullptr;

constexpr size_t kOutputQueueSamples = kFrameSamples * 64u;
static int16_t s_output_queue[kOutputQueueSamples];
static size_t s_output_queue_head = 0;
static size_t s_output_queue_tail = 0;
static size_t s_output_queue_count = 0;
static SemaphoreHandle_t s_output_queue_mutex = nullptr;

#if ENABLE_ES8311_TX_DTMF_TEST
struct DtmfGeneratorState {
    uint8_t digit = 0;
    bool gap = false;
    size_t remaining = 0;
    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float step1 = 0.0f;
    float step2 = 0.0f;
};

static bool dtmf_digit_freqs(const uint8_t digit, float *low_hz, float *high_hz) {
    switch (digit) {
        case 0:
            *low_hz = 941.0f; *high_hz = 1336.0f; return true;
        case 1:
            *low_hz = 697.0f; *high_hz = 1209.0f; return true;
        case 2:
            *low_hz = 697.0f; *high_hz = 1336.0f; return true;
        case 3:
            *low_hz = 697.0f; *high_hz = 1477.0f; return true;
        case 4:
            *low_hz = 770.0f; *high_hz = 1209.0f; return true;
        case 5:
            *low_hz = 770.0f; *high_hz = 1336.0f; return true;
        case 6:
            *low_hz = 770.0f; *high_hz = 1477.0f; return true;
        case 7:
            *low_hz = 852.0f; *high_hz = 1209.0f; return true;
        case 8:
            *low_hz = 852.0f; *high_hz = 1336.0f; return true;
        case 9:
            *low_hz = 852.0f; *high_hz = 1477.0f; return true;
        default:
            return false;
    }
}

static void dtmf_set_digit(DtmfGeneratorState *state, const uint8_t digit) {
    float low = 0.0f;
    float high = 0.0f;
    if (!dtmf_digit_freqs(digit, &low, &high)) {
        state->step1 = 0.0f;
        state->step2 = 0.0f;
        return;
    }
    state->step1 = (kTwoPi * low) / static_cast<float>(kSampleRate);
    state->step2 = (kTwoPi * high) / static_cast<float>(kSampleRate);
}

static void dtmf_reset(DtmfGeneratorState *state) {
    state->digit = 0;
    state->gap = false;
    state->remaining = kDtmfToneSamples;
    state->phase1 = 0.0f;
    state->phase2 = 0.0f;
    dtmf_set_digit(state, state->digit);
}

static void dtmf_next_segment(DtmfGeneratorState *state) {
    if (state->gap) {
        state->gap = false;
        state->digit = static_cast<uint8_t>((state->digit + 1u) % 10u);
        state->remaining = kDtmfToneSamples;
        dtmf_set_digit(state, state->digit);
    } else {
        state->gap = true;
        state->remaining = kDtmfGapSamples;
    }
}

static void dtmf_fill_frame(DtmfGeneratorState *state, int16_t *dst, const size_t samples) {
    for (size_t i = 0; i < samples; ++i) {
        if (state->remaining == 0) {
            dtmf_next_segment(state);
        }

        int16_t sample = 0;
        if (!state->gap) {
            const float mixed = 0.5f * (sinf(state->phase1) + sinf(state->phase2));
            sample = static_cast<int16_t>(mixed * kDtmfAmplitude * static_cast<float>(INT16_MAX));

            state->phase1 += state->step1;
            state->phase2 += state->step2;
            if (state->phase1 >= kTwoPi) {
                state->phase1 -= kTwoPi;
            }
            if (state->phase2 >= kTwoPi) {
                state->phase2 -= kTwoPi;
            }
        }

        dst[i] = sample;
        --state->remaining;
    }
}
#endif

static bool es8311_write_reg(uint8_t reg, uint8_t value) {
    bool ok = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        I2C_Start();
        ok = true;
        if (I2C_Write((kEs8311Addr << 1) | I2C_WRITE) < 0) {
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
            return true;
        }
        delay(1);
    }
    return false;
}

static uint8_t es8311_resolution_value(const int bits) {
    switch (bits) {
        case 16:
            return (3u << 2);
        case 18:
            return (2u << 2);
        case 20:
            return (1u << 2);
        case 24:
            return (0u << 2);
        case 32:
            return (4u << 2);
        default:
            return (3u << 2);
    }
}

static uint8_t es8311_pre_mult_code(const uint8_t pre_mult) {
    switch (pre_mult) {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            return 0;
    }
}

static bool es8311_reset(void) {
    if (!es8311_write_reg(ES8311_REG00_RESET, kEs8311ResetAll)) {
        return false;
    }
    delay(5);
    if (!es8311_write_reg(ES8311_REG00_RESET, kEs8311ResetRelease)) {
        return false;
    }
    delay(5);
    return true;
}

static bool es8311_config_clock(const Es8311ClockConfig &cfg) {
    if (!es8311_write_reg(ES8311_REG01_CLK_MANAGER, kEs8311ClockEnableAll)) {
        return false;
    }

    uint8_t reg02 = 0x00;
    reg02 |= static_cast<uint8_t>((cfg.pre_div - 1u) << 5);
    reg02 |= static_cast<uint8_t>(es8311_pre_mult_code(cfg.pre_mult) << 3);
    if (!es8311_write_reg(ES8311_REG02_CLK_MANAGER, reg02)) {
        return false;
    }

    if (!es8311_write_reg(ES8311_REG03_CLK_MANAGER, static_cast<uint8_t>((cfg.fs_mode << 6) | cfg.adc_osr))) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG04_CLK_MANAGER, cfg.dac_osr)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG05_CLK_MANAGER,
                          static_cast<uint8_t>(((cfg.adc_div - 1u) << 4) | (cfg.dac_div - 1u)))) {
        return false;
    }

    uint8_t reg06 = 0x00;
    if (cfg.bclk_div < 19u) {
        reg06 |= static_cast<uint8_t>(cfg.bclk_div - 1u);
    } else {
        reg06 |= cfg.bclk_div;
    }

    if (!es8311_write_reg(ES8311_REG06_CLK_MANAGER, reg06)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG07_CLK_MANAGER, cfg.lrck_h)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG08_CLK_MANAGER, cfg.lrck_l)) {
        return false;
    }
    return true;
}

static bool es8311_config_i2s_format(const int bits) {
    const uint8_t reg_value = es8311_resolution_value(bits);
    if (!es8311_write_reg(ES8311_REG09_SDPIN, reg_value)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG0A_SDPOUT, reg_value)) {
        return false;
    }
    return true;
}

static bool es8311_config_analog_mic(const uint8_t reg14, const uint8_t adc_gain_scale, const uint8_t adc_volume) {
    if (!es8311_write_reg(ES8311_REG14_SYSTEM, reg14)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG16_ADC, adc_gain_scale)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG17_ADC, adc_volume)) {
        return false;
    }
    return true;
}

static bool es8311_set_dac_volume(const uint8_t volume) {
    return es8311_write_reg(ES8311_REG32_DAC, volume);
}

static bool es8311_power_up_defaults(void) {
    if (!es8311_write_reg(ES8311_REG0D_SYSTEM, kEs8311PowerUpAnalog)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG0E_SYSTEM, kEs8311PowerUpPgaAdc)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG12_SYSTEM, kEs8311PowerUpDac)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG13_SYSTEM, kEs8311HpDriveEnable)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG1C_ADC, kEs8311AdcEqBypass)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG37_DAC, kEs8311DacEqBypass)) {
        return false;
    }
    return true;
}

static bool es8311_power_on(void) {
    return es8311_write_reg(ES8311_REG00_RESET, kEs8311PowerOn);
}

static bool es8311_configure_codec(void) {
    if (!es8311_write_reg(ES8311_REG44_GPIO, 0x08)) {
        return false;
    }
    if (!es8311_write_reg(ES8311_REG44_GPIO, 0x08)) {
        return false;
    }

    if (!es8311_reset()) {
        return false;
    }

    const Es8311ClockConfig clock_cfg = {
        .pre_div = 0x01,
        .pre_mult = 0x01,
        .adc_div = 0x01,
        .dac_div = 0x01,
        .fs_mode = 0x00,
        .lrck_h = 0x00,
        .lrck_l = 0xFF,
        .bclk_div = 0x04,
        .adc_osr = 0x10,
        .dac_osr = 0x10,
    };

    if (!es8311_config_clock(clock_cfg)) {
        return false;
    }

    if (!es8311_config_i2s_format(16)) {
        return false;
    }

    if (!es8311_config_analog_mic(kEs8311AnalogMicPgaEnable,
                                  kEs8311AdcGainScaleUp,
                                  kEs8311AdcVolumeDefault)) {
        return false;
    }

    if (!es8311_set_dac_volume(kEs8311DacVolumeDefault)) {
        return false;
    }

    if (!es8311_power_up_defaults()) {
        return false;
    }

    return es8311_power_on();
}

static bool i2s_setup(void) {
    if (s_i2s_ready) {
        return true;
    }

    if (s_i2s_driver_installed) {
        i2s_driver_uninstall(kI2sPort);
        s_i2s_driver_installed = false;
    }

    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    config.sample_rate = kSampleRate;
    config.bits_per_sample = kBitsPerSample;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = 0;
    config.dma_buf_count = 8;
    config.dma_buf_len = kFrameSamples;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;

    if (i2s_driver_install(kI2sPort, &config, 0, nullptr) != ESP_OK) {
        return false;
    }
    s_i2s_driver_installed = true;

    i2s_pin_config_t pin_config = {};
    pin_config.mck_io_num = kPinMclk;
    pin_config.bck_io_num = kPinBclk;
    pin_config.ws_io_num = kPinLrclk;
    pin_config.data_out_num = kPinDin;
    pin_config.data_in_num = kPinDout;

    if (i2s_set_pin(kI2sPort, &pin_config) != ESP_OK) {
        i2s_driver_uninstall(kI2sPort);
        s_i2s_driver_installed = false;
        return false;
    }

    if (i2s_set_clk(kI2sPort, kSampleRate, kBitsPerSample, I2S_CHANNEL_MONO) != ESP_OK) {
        i2s_driver_uninstall(kI2sPort);
        s_i2s_driver_installed = false;
        return false;
    }

    i2s_zero_dma_buffer(kI2sPort);
    s_i2s_ready = true;
    return true;
}

static bool i2s_read_frame(int16_t *dst) {
    size_t bytes_in_frame = 0;
    while (bytes_in_frame < kFrameBytes) {
        size_t bytes_read = 0;
        if (i2s_read(kI2sPort,
                     reinterpret_cast<uint8_t *>(dst) + bytes_in_frame,
                     kFrameBytes - bytes_in_frame,
                     &bytes_read,
                     kI2sWaitTicks) != ESP_OK) {
            return false;
        }

        if (bytes_read == 0) {
            continue;
        }

        bytes_in_frame += bytes_read;
    }
    return true;
}

static bool i2s_write_frame(const int16_t *src) {
    size_t bytes_out_frame = 0;
    while (bytes_out_frame < kFrameBytes) {
        size_t bytes_written = 0;
        if (i2s_write(kI2sPort,
                      reinterpret_cast<const uint8_t *>(src) + bytes_out_frame,
                      kFrameBytes - bytes_out_frame,
                      &bytes_written,
                      kI2sWaitTicks) != ESP_OK) {
            return false;
        }

        if (bytes_written == 0) {
            continue;
        }

        bytes_out_frame += bytes_written;
    }
    return true;
}

static void output_queue_init(void) {
    if (s_output_queue_mutex == nullptr) {
        s_output_queue_mutex = xSemaphoreCreateMutex();
    }
}

static void output_queue_clear_locked(void) {
    s_output_queue_head = 0;
    s_output_queue_tail = 0;
    s_output_queue_count = 0;
}

static size_t output_queue_push(const int16_t *samples, size_t sample_count) {
    if (samples == nullptr || sample_count == 0) {
        return 0;
    }

    output_queue_init();
    if (s_output_queue_mutex == nullptr) {
        return 0;
    }

    if (xSemaphoreTake(s_output_queue_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return 0;
    }

    size_t written = 0;
    while (written < sample_count && s_output_queue_count < kOutputQueueSamples) {
        s_output_queue[s_output_queue_tail] = samples[written++];
        s_output_queue_tail = (s_output_queue_tail + 1u) % kOutputQueueSamples;
        ++s_output_queue_count;
    }

    xSemaphoreGive(s_output_queue_mutex);
    return written;
}

static size_t output_queue_pop_frame(int16_t *dst, const size_t sample_count) {
    if (dst == nullptr || sample_count == 0) {
        return 0;
    }

    output_queue_init();
    if (s_output_queue_mutex == nullptr) {
        memset(dst, 0, sample_count * sizeof(int16_t));
        return 0;
    }

    if (xSemaphoreTake(s_output_queue_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        memset(dst, 0, sample_count * sizeof(int16_t));
        return 0;
    }

    size_t read = 0;
    while (read < sample_count && s_output_queue_count > 0) {
        dst[read++] = s_output_queue[s_output_queue_head];
        s_output_queue_head = (s_output_queue_head + 1u) % kOutputQueueSamples;
        --s_output_queue_count;
    }

    xSemaphoreGive(s_output_queue_mutex);

    if (read < sample_count) {
        memset(dst + read, 0, (sample_count - read) * sizeof(int16_t));
    }

    return read;
}

static void output_queue_clear(void) {
    output_queue_init();
    if (s_output_queue_mutex == nullptr) {
        return;
    }

    if (xSemaphoreTake(s_output_queue_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return;
    }
    output_queue_clear_locked();
    xSemaphoreGive(s_output_queue_mutex);
}

static void es8311_passthrough_task(void *) {
    static int16_t frame[kFrameSamples];
#if ENABLE_ES8311_TX_DTMF_TEST
    DtmfGeneratorState dtmf = {};
    dtmf_reset(&dtmf);
    ES8311_AudioMode_t last_mode = s_audio_mode;
#endif

    while (s_passthrough_running) {
#if ENABLE_ES8311_TX_DTMF_TEST
        const ES8311_AudioMode_t mode = s_audio_mode;
        if (mode != last_mode) {
            last_mode = mode;
            if (mode == ES8311_AUDIO_MODE_TRANSMIT) {
                dtmf_reset(&dtmf);
            }
        }

        if (mode == ES8311_AUDIO_MODE_TRANSMIT) {
            if (output_queue_pop_frame(frame, kFrameSamples) == 0) {
                dtmf_fill_frame(&dtmf, frame, kFrameSamples);
            }
            if (!i2s_write_frame(frame)) {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            continue;
        }
#endif

        if (mode == ES8311_AUDIO_MODE_TRANSMIT) {
            output_queue_pop_frame(frame, kFrameSamples);
            if (!i2s_write_frame(frame)) {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
            continue;
        }

        if (!i2s_read_frame(frame)) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (s_frame_hook != nullptr) {
            s_frame_hook(frame, kFrameSamples, mode, s_frame_hook_user_data);
        }

        if (!i2s_write_frame(frame)) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
    }

    i2s_zero_dma_buffer(kI2sPort);
    s_passthrough_task = nullptr;
    vTaskDelete(nullptr);
}

static bool es8311_start_passthrough(void) {
    if (!s_i2s_ready && !i2s_setup()) {
        return false;
    }

    if (s_passthrough_task != nullptr) {
        return true;
    }

    s_passthrough_running = true;
    if (xTaskCreate(es8311_passthrough_task,
                    "es8311_passthrough",
                    4096,
                    nullptr,
                    3,
                    &s_passthrough_task) != pdPASS) {
        s_passthrough_running = false;
        s_passthrough_task = nullptr;
        return false;
    }

    return true;
}

static void es8311_stop_passthrough(void) {
    if (s_passthrough_task == nullptr) {
        return;
    }

    s_passthrough_running = false;
    for (int wait = 0; wait < 50 && s_passthrough_task != nullptr; ++wait) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static inline void es8311_apply_switch_level(const ES8311_AudioMode_t mode) {
    digitalWrite(kPinAudioSwitch, mode == ES8311_AUDIO_MODE_TRANSMIT ? HIGH : LOW);
    s_audio_mode = mode;
}

} // namespace

bool ES8311_Init(void) {
    if (s_es8311_ready) {
        return es8311_start_passthrough();
    }

    pinMode(kPinAudioSwitch, OUTPUT);
    es8311_apply_switch_level(ES8311_AUDIO_MODE_RECEIVE);

    I2C_Init();

    if (!es8311_configure_codec()) {
        s_es8311_ready = false;
        return false;
    }

    if (!i2s_setup()) {
        s_es8311_ready = false;
        return false;
    }

    if (!es8311_start_passthrough()) {
        s_es8311_ready = false;
        return false;
    }

    s_es8311_ready = true;
    return true;
}

bool ES8311_IsReady(void) {
    return s_es8311_ready;
}

bool ES8311_SetAudioMode(const ES8311_AudioMode_t mode) {
    if (!ES8311_Init()) {
        return false;
    }

    es8311_apply_switch_level(mode);
    return true;
}

bool ES8311_SetReceiveMode(void) {
    return ES8311_SetAudioMode(ES8311_AUDIO_MODE_RECEIVE);
}

bool ES8311_SetTransmitMode(void) {
    return ES8311_SetAudioMode(ES8311_AUDIO_MODE_TRANSMIT);
}

void ES8311_SetFrameHook(ES8311_FrameHook_t hook, void *user_data) {
    s_frame_hook = hook;
    s_frame_hook_user_data = user_data;
}

size_t ES8311_QueueOutputSamples(const int16_t *samples, size_t sample_count) {
    if (!ES8311_Init()) {
        return 0;
    }

    return output_queue_push(samples, sample_count);
}

void ES8311_ClearOutputQueue(void) {
    output_queue_clear();
}

int ES8311_GetSampleRate(void) {
    return kSampleRate;
}

size_t ES8311_GetFrameSamples(void) {
    return kFrameSamples;
}

bool ES8311_PlayTestTone(const uint32_t durationMs) {
    if (!ES8311_Init()) {
        return false;
    }

    const ES8311_AudioMode_t previous_mode = s_audio_mode;
    const bool was_passthrough_running = (s_passthrough_task != nullptr);
    if (was_passthrough_running) {
        es8311_stop_passthrough();
    }

    // test tone should be heard from speaker path
    es8311_apply_switch_level(ES8311_AUDIO_MODE_RECEIVE);

    static int16_t frame[kFrameSamples];
    size_t total_samples = (static_cast<size_t>(kSampleRate) * durationMs) / 1000u;
    if (total_samples == 0) {
        total_samples = kFrameSamples;
    }

    float phase = 0.0f;
    const float step = (kTwoPi * kToneFrequency) / static_cast<float>(kSampleRate);

    bool ok = true;
    size_t produced = 0;
    while (produced < total_samples) {
        const size_t samples_this = (total_samples - produced < kFrameSamples)
                                      ? (total_samples - produced)
                                      : kFrameSamples;

        for (size_t i = 0; i < samples_this; ++i) {
            const int16_t sample = static_cast<int16_t>(sinf(phase) * kToneAmplitude * static_cast<float>(INT16_MAX));
            frame[i] = sample;

            phase += step;
            if (phase >= kTwoPi) {
                phase -= kTwoPi;
            }
        }

        if (samples_this < kFrameSamples) {
            memset(frame + samples_this, 0, (kFrameSamples - samples_this) * sizeof(int16_t));
        }

        if (!i2s_write_frame(frame)) {
            ok = false;
            break;
        }

        produced += samples_this;
    }

    i2s_zero_dma_buffer(kI2sPort);

    if (was_passthrough_running && !es8311_start_passthrough()) {
        ok = false;
    }

    if (previous_mode != ES8311_AUDIO_MODE_RECEIVE) {
        es8311_apply_switch_level(previous_mode);
    }

    return ok;
}

bool ES8311_RecordMicAndPlayback(uint32_t durationMs) {
    if (!ES8311_Init()) {
        return false;
    }

    if (durationMs == 0U) {
        durationMs = 3000U;
    }

    size_t total_samples = (static_cast<size_t>(kSampleRate) * durationMs) / 1000U;
    if (total_samples == 0U) {
        total_samples = kFrameSamples;
    }

    int16_t *recorded = static_cast<int16_t *>(malloc(total_samples * sizeof(int16_t)));
    if (!recorded) {
        return false;
    }

    const ES8311_AudioMode_t previous_mode = s_audio_mode;
    const bool was_passthrough_running = (s_passthrough_task != nullptr);
    if (was_passthrough_running) {
        es8311_stop_passthrough();
    }

    bool ok = true;
    static int16_t frame[kFrameSamples];
    size_t captured = 0U;

    // Capture mic audio.
    es8311_apply_switch_level(ES8311_AUDIO_MODE_TRANSMIT);
    i2s_zero_dma_buffer(kI2sPort);
    while (captured < total_samples) {
        if (!i2s_read_frame(frame)) {
            ok = false;
            break;
        }

        const size_t samples_this = ((total_samples - captured) < kFrameSamples)
                                        ? (total_samples - captured)
                                        : kFrameSamples;
        memcpy(recorded + captured, frame, samples_this * sizeof(int16_t));
        captured += samples_this;
    }

    // Playback captured audio to speaker.
    if (ok) {
        es8311_apply_switch_level(ES8311_AUDIO_MODE_RECEIVE);
        i2s_zero_dma_buffer(kI2sPort);

        size_t played = 0U;
        while (played < captured) {
            const size_t samples_this = ((captured - played) < kFrameSamples)
                                            ? (captured - played)
                                            : kFrameSamples;

            if (samples_this < kFrameSamples) {
                memcpy(frame, recorded + played, samples_this * sizeof(int16_t));
                memset(frame + samples_this, 0, (kFrameSamples - samples_this) * sizeof(int16_t));
            } else {
                memcpy(frame, recorded + played, kFrameBytes);
            }

            if (!i2s_write_frame(frame)) {
                ok = false;
                break;
            }
            played += samples_this;
        }

        i2s_zero_dma_buffer(kI2sPort);
    }

    free(recorded);

    if (was_passthrough_running && !es8311_start_passthrough()) {
        ok = false;
    }

    if (s_audio_mode != previous_mode) {
        es8311_apply_switch_level(previous_mode);
    }

    return ok;
}
