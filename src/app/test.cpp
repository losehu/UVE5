#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "audio.h"
#include "driver/i2s.h"




static const int kPinDin = 36;   // codec DIN, data from MCU
static const int kPinDout = 38;  // codec DOUT, data to MCU
static const int kPinBclk = 42;
static const int kPinLrclk = 37;
static const int kPinMclk = 35;
static const int kPinScl = 14;
static const int kPinSda = 13;
static const int kPinAudioSwitch = 41; // HIGH: mic input path, LOW: speaker output path

static const uint8_t kEs8311Addr = 0x18; // 7-bit I2C address
static const int kSampleRate = 16000;
static const int kBitsPerSample = 16;
static const int kFrameSamples = 160;
static const int kI2sWaitTicks = 0;
static const int kRecordSeconds = 3;

// ES8311 registers (subset)
static const uint8_t ES8311_REG00_RESET = 0x00;
static const uint8_t ES8311_REG01_CLK_MANAGER = 0x01;
static const uint8_t ES8311_REG02_CLK_MANAGER = 0x02;
static const uint8_t ES8311_REG03_CLK_MANAGER = 0x03;
static const uint8_t ES8311_REG04_CLK_MANAGER = 0x04;
static const uint8_t ES8311_REG05_CLK_MANAGER = 0x05;
static const uint8_t ES8311_REG06_CLK_MANAGER = 0x06;
static const uint8_t ES8311_REG07_CLK_MANAGER = 0x07;
static const uint8_t ES8311_REG08_CLK_MANAGER = 0x08;
static const uint8_t ES8311_REG09_SDPIN = 0x09;
static const uint8_t ES8311_REG0A_SDPOUT = 0x0A;
static const uint8_t ES8311_REG0D_SYSTEM = 0x0D;
static const uint8_t ES8311_REG0E_SYSTEM = 0x0E;
static const uint8_t ES8311_REG12_SYSTEM = 0x12;
static const uint8_t ES8311_REG13_SYSTEM = 0x13;
static const uint8_t ES8311_REG14_SYSTEM = 0x14;
static const uint8_t ES8311_REG16_ADC = 0x16;
static const uint8_t ES8311_REG17_ADC = 0x17;
static const uint8_t ES8311_REG1C_ADC = 0x1C;
static const uint8_t ES8311_REG32_DAC = 0x32;
static const uint8_t ES8311_REG37_DAC = 0x37;

static const uint8_t kEs8311ResetAll = 0x1F;
static const uint8_t kEs8311ResetRelease = 0x00;
static const uint8_t kEs8311PowerOn = 0x80;
static const uint8_t kEs8311ClockEnableAll = 0x3F;
// ES8311 Reg0x14: LINSEL=Mic1p-Mic1n (0x10) + PGAGAIN.
// 0x1A sets PGAGAIN=10 (30dB, max). Use a more moderate mic PGA gain.
static const uint8_t kEs8311AnalogMicPgaEnable = 0x1A; // 15dB
static const uint8_t kEs8311AdcGainScaleUp = 0x24;
static const uint8_t kEs8311AdcVolumeDefault = 0xe0;
static const uint8_t kEs8311DacVolumeDefault = 0x7F;
static const uint8_t kEs8311PowerUpAnalog = 0x01;
static const uint8_t kEs8311PowerUpPgaAdc = 0x02;
static const uint8_t kEs8311PowerUpDac = 0x00;
static const uint8_t kEs8311HpDriveEnable = 0x10;
static const uint8_t kEs8311AdcEqBypass = 0x6A;
static const uint8_t kEs8311DacEqBypass = 0x08;

static const size_t kFrameBytes = kFrameSamples * (kBitsPerSample / 8);
static constexpr size_t kRecordSamples = static_cast<size_t>(kSampleRate) * static_cast<size_t>(kRecordSeconds);
static int16_t g_frame[kFrameSamples];
static int16_t g_audio[kRecordSamples];
static size_t g_audio_pos_samples = 0;

static void serial_drain_input(uint32_t duration_ms) {
  const uint32_t start = millis();
  while (millis() - start < duration_ms) {
    while (Serial.available() > 0) {
      (void)Serial.read();
    }
    delay(1);
  }
}

enum class AudioMode : uint8_t {
  kIdle = 0,
  kRecording = 1,
  kPlaying = 2,
};

static AudioMode g_mode = AudioMode::kIdle;

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

static void i2c_delay() {
  delayMicroseconds(4);
}

static void i2c_sda_high() {
  digitalWrite(kPinSda, HIGH);
}

static void i2c_sda_low() {
  digitalWrite(kPinSda, LOW);
}

static void i2c_scl_high() {
  digitalWrite(kPinScl, HIGH);
}

static void i2c_scl_low() {
  digitalWrite(kPinScl, LOW);
}

static void i2c_start() {
  i2c_sda_high();
  i2c_scl_high();
  i2c_delay();
  i2c_sda_low();
  i2c_delay();
  i2c_scl_low();
}

static void i2c_stop() {
  i2c_sda_low();
  i2c_delay();
  i2c_scl_high();
  i2c_delay();
  i2c_sda_high();
  i2c_delay();
}

static bool i2c_write_byte(uint8_t data) {
  for (int i = 0; i < 8; ++i) {
    if (data & 0x80) {
      i2c_sda_high();
    } else {
      i2c_sda_low();
    }
    i2c_delay();
    i2c_scl_high();
    i2c_delay();
    i2c_scl_low();
    data <<= 1;
  }

  i2c_sda_high();
  i2c_delay();
  i2c_scl_high();
  i2c_delay();
  bool ack = (digitalRead(kPinSda) == LOW);
  i2c_scl_low();
  return ack;
}

static uint8_t i2c_read_byte(bool ack) {
  uint8_t data = 0;
  i2c_sda_high();
  for (int i = 0; i < 8; ++i) {
    data <<= 1;
    i2c_scl_high();
    i2c_delay();
    if (digitalRead(kPinSda)) {
      data |= 0x01;
    }
    i2c_scl_low();
    i2c_delay();
  }

  if (ack) {
    i2c_sda_low();
  } else {
    i2c_sda_high();
  }
  i2c_delay();
  i2c_scl_high();
  i2c_delay();
  i2c_scl_low();
  i2c_sda_high();
  return data;
}

static bool es8311_write_reg(uint8_t reg, uint8_t value) {
  i2c_start();
  if (!i2c_write_byte((kEs8311Addr << 1) | 0x00)) {
    i2c_stop();
    return false;
  }
  if (!i2c_write_byte(reg)) {
    i2c_stop();
    return false;
  }
  if (!i2c_write_byte(value)) {
    i2c_stop();
    return false;
  }
  i2c_stop();
  return true;
}

static uint8_t es8311_resolution_value(int bits) {
  switch (bits) {
    case 16:
      return (3 << 2);
    case 18:
      return (2 << 2);
    case 20:
      return (1 << 2);
    case 24:
      return (0 << 2);
    case 32:
      return (4 << 2);
    default:
      return (3 << 2);
  }
}

static uint8_t es8311_pre_mult_code(uint8_t pre_mult) {
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

static bool es8311_reset() {
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
  reg02 |= (cfg.pre_div - 1) << 5;
  reg02 |= (es8311_pre_mult_code(cfg.pre_mult) << 3);
  if (!es8311_write_reg(ES8311_REG02_CLK_MANAGER, reg02)) {
    return false;
  }

  if (!es8311_write_reg(ES8311_REG03_CLK_MANAGER, (cfg.fs_mode << 6) | cfg.adc_osr)) {
    return false;
  }
  if (!es8311_write_reg(ES8311_REG04_CLK_MANAGER, cfg.dac_osr)) {
    return false;
  }
  if (!es8311_write_reg(ES8311_REG05_CLK_MANAGER, ((cfg.adc_div - 1) << 4) | (cfg.dac_div - 1))) {
    return false;
  }

  uint8_t reg06 = 0x00;
  if (cfg.bclk_div < 19) {
    reg06 |= (cfg.bclk_div - 1);
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

static bool es8311_config_i2s_format(int bits) {
  uint8_t reg_value = es8311_resolution_value(bits);
  if (!es8311_write_reg(ES8311_REG09_SDPIN, reg_value)) {
    return false;
  }
  if (!es8311_write_reg(ES8311_REG0A_SDPOUT, reg_value)) {
    return false;
  }
  return true;
}

static bool es8311_config_analog_mic(uint8_t reg14, uint8_t adc_gain_scale, uint8_t adc_volume) {
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

static bool es8311_set_dac_volume(uint8_t volume) {
  return es8311_write_reg(ES8311_REG32_DAC, volume);
}

static bool es8311_power_up_defaults() {
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

static bool es8311_power_on() {
  return es8311_write_reg(ES8311_REG00_RESET, kEs8311PowerOn);
}

static bool es8311_init() {
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

  if (!es8311_config_i2s_format(kBitsPerSample)) {
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

static bool i2s_setup() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  config.sample_rate = kSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = 8;
  config.dma_buf_len = kFrameSamples;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) {
    return false;
  }

  i2s_pin_config_t pin_config = {};
  pin_config.mck_io_num = kPinMclk;
  pin_config.bck_io_num = kPinBclk;
  pin_config.ws_io_num = kPinLrclk;
  pin_config.data_out_num = kPinDin;
  pin_config.data_in_num = kPinDout;

  if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
    return false;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

static bool i2s_read_frame(int16_t *dst) {
  size_t bytes_in_frame = 0;
  while (bytes_in_frame < kFrameBytes) {
    size_t to_read = kFrameBytes - bytes_in_frame;
    size_t bytes_read = 0;
    if (i2s_read(I2S_NUM_0, reinterpret_cast<uint8_t *>(dst) + bytes_in_frame,
                 to_read, &bytes_read, kI2sWaitTicks) != ESP_OK) {
      return false;
    }
    if (bytes_read == 0) {
      delay(1);
      continue;
    }
    bytes_in_frame += bytes_read;
  }
  return true;
}

static bool i2s_write_frame(const int16_t *src) {
  size_t bytes_out_frame = 0;
  while (bytes_out_frame < kFrameBytes) {
    size_t to_write = kFrameBytes - bytes_out_frame;
    size_t bytes_written = 0;
    if (i2s_write(I2S_NUM_0, reinterpret_cast<const uint8_t *>(src) + bytes_out_frame,
                  to_write, &bytes_written, kI2sWaitTicks) != ESP_OK) {
      return false;
    }
    if (bytes_written == 0) {
      delay(1);
      continue;
    }
    bytes_out_frame += bytes_written;
  }
  return true;
}

static void audio_abort_to_idle(const __FlashStringHelper *reason) {
  g_mode = AudioMode::kIdle;
  g_audio_pos_samples = 0;
  i2s_zero_dma_buffer(I2S_NUM_0);
  digitalWrite(kPinAudioSwitch, LOW);
  AUDIO_AudioPathOff();
  Serial.println(reason);
}

static void audio_start_cycle() {
  g_mode = AudioMode::kRecording;
  g_audio_pos_samples = 0;
  i2s_zero_dma_buffer(I2S_NUM_0);
  digitalWrite(kPinAudioSwitch, HIGH);
  AUDIO_AudioPathOff();
  Serial.print("Recording ");
  Serial.print(kRecordSeconds);
  Serial.println("s...");
}

static bool dtmf_digit_freqs(char digit, float *low_hz, float *high_hz) {
  switch (digit) {
    case '1': *low_hz = 697.0f; *high_hz = 1209.0f; return true;
    case '2': *low_hz = 697.0f; *high_hz = 1336.0f; return true;
    case '3': *low_hz = 697.0f; *high_hz = 1477.0f; return true;
    case '4': *low_hz = 770.0f; *high_hz = 1209.0f; return true;
    case '5': *low_hz = 770.0f; *high_hz = 1336.0f; return true;
    case '6': *low_hz = 770.0f; *high_hz = 1477.0f; return true;
    case '7': *low_hz = 852.0f; *high_hz = 1209.0f; return true;
    case '8': *low_hz = 852.0f; *high_hz = 1336.0f; return true;
    case '9': *low_hz = 852.0f; *high_hz = 1477.0f; return true;
    case '0': *low_hz = 941.0f; *high_hz = 1336.0f; return true;
    default: break;
  }
  return false;
}

static bool dtmf_write_silence(uint32_t duration_ms) {
  const size_t total_samples = (static_cast<size_t>(kSampleRate) * duration_ms) / 1000u;
  size_t written = 0;
  memset(g_frame, 0, sizeof(g_frame));
  while (written < total_samples) {
    const size_t remaining = total_samples - written;
    const size_t samples_this = (remaining < static_cast<size_t>(kFrameSamples))
                                  ? remaining
                                  : static_cast<size_t>(kFrameSamples);
    if (samples_this != static_cast<size_t>(kFrameSamples)) {
      memset(g_frame, 0, sizeof(g_frame));
    }
    if (!i2s_write_frame(g_frame)) {
      return false;
    }
    written += samples_this;
  }
  return true;
}

static bool dtmf_play_digit(char digit, uint32_t tone_ms) {
  float low_hz = 0.0f;
  float high_hz = 0.0f;
  if (!dtmf_digit_freqs(digit, &low_hz, &high_hz)) {
    return false;
  }

  static constexpr float kTwoPi = 6.28318530717958647692f;
  static constexpr float kAmp = 0.22f;  // keep headroom to avoid clipping

  const float step1 = kTwoPi * low_hz / static_cast<float>(kSampleRate);
  const float step2 = kTwoPi * high_hz / static_cast<float>(kSampleRate);
  float phase1 = 0.0f;
  float phase2 = 0.0f;

  const size_t total_samples = (static_cast<size_t>(kSampleRate) * tone_ms) / 1000u;
  size_t produced = 0;
  while (produced < total_samples) {
    const size_t remaining = total_samples - produced;
    const size_t samples_this = (remaining < static_cast<size_t>(kFrameSamples))
                                  ? remaining
                                  : static_cast<size_t>(kFrameSamples);

    for (size_t i = 0; i < samples_this; ++i) {
      const float s = 0.5f * (sinf(phase1) + sinf(phase2));
      const float scaled = s * kAmp;
      float clamped = scaled;
      if (clamped > 1.0f) clamped = 1.0f;
      if (clamped < -1.0f) clamped = -1.0f;
      g_frame[i] = static_cast<int16_t>(clamped * 32767.0f);

      phase1 += step1;
      phase2 += step2;
      if (phase1 >= kTwoPi) phase1 -= kTwoPi;
      if (phase2 >= kTwoPi) phase2 -= kTwoPi;
    }
    if (samples_this != static_cast<size_t>(kFrameSamples)) {
      for (size_t i = samples_this; i < static_cast<size_t>(kFrameSamples); ++i) {
        g_frame[i] = 0;
      }
    }

    if (!i2s_write_frame(g_frame)) {
      return false;
    }

    produced += samples_this;

    while (Serial.available() > 0) {
      const int raw = Serial.read();
      if (raw < 0) {
        break;
      }
      const char c = static_cast<char>(raw);
      if (c == 's' || c == 'S') {
        Serial.println("Stopped");
        return false;
      }
    }
  }

  return true;
}

static void dtmf_play_0_to_9_once() {
  if (g_mode != AudioMode::kIdle) {
    audio_abort_to_idle(F("Stopped"));
  }
  Serial.println("DTMF 0..9");
  digitalWrite(kPinAudioSwitch, LOW);
  AUDIO_AudioPathOn();
  i2s_zero_dma_buffer(I2S_NUM_0);

  for (char digit = '0'; digit <= '9'; ++digit) {
    if (!dtmf_play_digit(digit, 180)) {
      AUDIO_AudioPathOff();
      return;
    }
    if (!dtmf_write_silence(60)) {
      Serial.println("I2S write failed");
      AUDIO_AudioPathOff();
      return;
    }
  }

  Serial.println("Done");
  AUDIO_AudioPathOff();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  // After flashing/reset, host tools may leave bytes queued in RX.
  // Drain them so we don't accidentally treat them as user commands.
  serial_drain_input(50);
  Serial.println("ES8311 record(3s)->play(3s)");

  pinMode(kPinAudioSwitch, OUTPUT);

  pinMode(AUDIO_PATH, OUTPUT);
  AUDIO_AudioPathOff();

  pinMode(kPinScl, OUTPUT_OPEN_DRAIN);
  pinMode(kPinSda, OUTPUT_OPEN_DRAIN);
  digitalWrite(kPinScl, HIGH);
  digitalWrite(kPinSda, HIGH);

  if (!es8311_init()) {
    Serial.println("ES8311 init failed (check I2C wiring/pullups)");
  } else {
    Serial.println("ES8311 init ok");       
  }

  if (!i2s_setup()) {
    Serial.println("I2S setup failed");
  } else {
    Serial.println("I2S setup ok");
  }

  Serial.println("Type 'r' to record 3s then play once");
  Serial.println("Type 'l' to play DTMF 0..9 once");
  Serial.println("Type 's' to stop");
  serial_drain_input(200);
}

void loop() {
  while (Serial.available() > 0) {
    const int raw = Serial.read();
    if (raw < 0) {
      break;
    }
    const char c = static_cast<char>(raw);
    if (c == 'r' || c == 'R') {
      audio_start_cycle();
      break;
    }
    if (c == 'l' || c == 'L') {
      dtmf_play_0_to_9_once();
      break;
    }
    if (c == 's' || c == 'S') {
      audio_abort_to_idle(F("Stopped"));
      break;
    }
  }

  if (g_mode == AudioMode::kIdle) {
    delay(5);
    return;
  }

  if (g_mode == AudioMode::kRecording) {
    const size_t samples_left = kRecordSamples - g_audio_pos_samples;
    const size_t samples_this = (samples_left < static_cast<size_t>(kFrameSamples))
                                  ? samples_left
                                  : static_cast<size_t>(kFrameSamples);

    if (samples_this == static_cast<size_t>(kFrameSamples)) {
      if (!i2s_read_frame(g_audio + g_audio_pos_samples)) {
        audio_abort_to_idle(F("I2S read failed"));
        delay(10);
        return;
      }
    } else {
      if (!i2s_read_frame(g_frame)) {
        audio_abort_to_idle(F("I2S read failed"));
        delay(10);
        return;
      }
      memcpy(g_audio + g_audio_pos_samples, g_frame, samples_this * sizeof(int16_t));
    }

    g_audio_pos_samples += samples_this;
    if (g_audio_pos_samples >= kRecordSamples) {
      g_mode = AudioMode::kPlaying;
      g_audio_pos_samples = 0;
      i2s_zero_dma_buffer(I2S_NUM_0);
      digitalWrite(kPinAudioSwitch, LOW);
      AUDIO_AudioPathOn();
      Serial.println("Recording done, playing...");
    }
    return;
  }

  if (g_mode == AudioMode::kPlaying) {
    const size_t samples_left = kRecordSamples - g_audio_pos_samples;
    const size_t samples_this = (samples_left < static_cast<size_t>(kFrameSamples))
                                  ? samples_left
                                  : static_cast<size_t>(kFrameSamples);

    if (samples_this == static_cast<size_t>(kFrameSamples)) {
      if (!i2s_write_frame(g_audio + g_audio_pos_samples)) {
        audio_abort_to_idle(F("I2S write failed"));
        delay(10);
        return;
      }
    } else {
      memset(g_frame, 0, sizeof(g_frame));
      memcpy(g_frame, g_audio + g_audio_pos_samples, samples_this * sizeof(int16_t));
      if (!i2s_write_frame(g_frame)) {
        audio_abort_to_idle(F("I2S write failed"));
        delay(10);
        return;
      }
    }

    g_audio_pos_samples += samples_this;
    if (g_audio_pos_samples >= kRecordSamples) {
      g_mode = AudioMode::kIdle;
      g_audio_pos_samples = 0;
      i2s_zero_dma_buffer(I2S_NUM_0);
      digitalWrite(kPinAudioSwitch, LOW);
      AUDIO_AudioPathOff();
      Serial.println("Done");
    }
    return;
  }
}
