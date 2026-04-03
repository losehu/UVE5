#include "nrl_audio_bridge.h"

#include "nrl_audio_config.h"
#include "wifi.h"

#include "app/app.h"
#include "audio.h"
#include "driver/es8311.h"
#include "functions.h"
#include "radio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

namespace {

constexpr uint8_t kNrlHeaderSize = 48u;
constexpr uint8_t kNrlTypeVoice = 1u;
constexpr uint8_t kNrlTypeHeartbeat = 2u;
constexpr uint8_t kNrlTypeServerVoice = 9u;
constexpr size_t kAudioCodecFrameSamples = 80u;
constexpr size_t kG711PayloadBytes = 160u;
constexpr size_t kG711RxPayloadMinBytes = 160u;
constexpr size_t kG711RxPayloadMaxBytes = 500u;
constexpr size_t kNrlMaxPacketSize = kNrlHeaderSize + 512u;

WiFiUDP s_udp;
SemaphoreHandle_t s_udp_mutex = nullptr;
TaskHandle_t s_bridge_task = nullptr;
bool s_bridge_initialized = false;
bool s_udp_started = false;
bool s_bridge_tx_active = false;
uint16_t s_packet_counter = 0u;
uint32_t s_last_rx_packet_ms = 0u;
uint32_t s_last_heartbeat_ms = 0u;
uint32_t s_last_remote_identity_ms = 0u;
IPAddress s_server_ip;
uint8_t s_cpu_id[4] = {};
bool s_alaw_tables_ready = false;
int16_t s_alaw_decode_table[256];
uint8_t s_alaw_encode_mag_table[4096];
char s_remote_callsign[7] = {};
uint8_t s_remote_ssid = 0u;

static void initCpuId(void) {
    memset(s_cpu_id, 0, sizeof(s_cpu_id));
}

static void updateRemoteIdentity(const uint8_t *packet, const size_t packet_size) {
    if (packet == nullptr || packet_size < 31u) {
        return;
    }

    memcpy(s_remote_callsign, packet + 24u, 6u);
    s_remote_callsign[6] = '\0';
    for (int i = 5; i >= 0; --i) {
        if (s_remote_callsign[i] == '\0' || s_remote_callsign[i] == '\r' || s_remote_callsign[i] == ' ') {
            s_remote_callsign[i] = '\0';
        } else {
            break;
        }
    }
    s_remote_ssid = packet[30];
    s_last_remote_identity_ms = millis();
}

static uint8_t linearToALawSlow(const int16_t pcm) {
    static const uint16_t seg_end[8] = {0x001F, 0x003F, 0x007F, 0x00FF, 0x01FF, 0x03FF, 0x07FF, 0x0FFF};

    uint8_t mask = 0xD5u;
    int32_t sample = pcm;
    if (sample < 0) {
        mask = 0x55u;
        sample = -sample - 1;
    }

    if (sample > 0x0FFF) {
        sample = 0x0FFF;
    }

    uint8_t seg = 0u;
    while (seg < 8u && static_cast<uint16_t>(sample) > seg_end[seg]) {
        ++seg;
    }

    uint8_t aval;
    if (seg >= 8u) {
        aval = 0x7Fu;
    } else {
        aval = static_cast<uint8_t>(seg << 4);
        if (seg < 2u) {
            aval |= static_cast<uint8_t>((sample >> 4) & 0x0Fu);
        } else {
            aval |= static_cast<uint8_t>((sample >> (seg + 3u)) & 0x0Fu);
        }
    }

    return aval ^ mask;
}

static int16_t aLawToLinearSlow(const uint8_t alaw) {
    uint8_t value = alaw ^ 0x55u;
    int16_t sample = static_cast<int16_t>((value & 0x0Fu) << 4);
    const uint8_t segment = static_cast<uint8_t>((value & 0x70u) >> 4);

    switch (segment) {
        case 0:
            sample += 8;
            break;
        case 1:
            sample += 0x108;
            break;
        default:
            sample += 0x108;
            sample <<= static_cast<uint8_t>(segment - 1u);
            break;
    }

    return (value & 0x80u) ? sample : static_cast<int16_t>(-sample);
}

static void initALawTables(void) {
    if (s_alaw_tables_ready) {
        return;
    }

    for (size_t i = 0; i < 256u; ++i) {
        s_alaw_decode_table[i] = aLawToLinearSlow(static_cast<uint8_t>(i));
    }

    for (size_t i = 0; i < 4096u; ++i) {
        s_alaw_encode_mag_table[i] = linearToALawSlow(static_cast<int16_t>(i));
    }

    s_alaw_tables_ready = true;
}

static uint8_t linearToALaw(const int16_t pcm) {
    if (!s_alaw_tables_ready) {
        initALawTables();
    }

    if (pcm >= 0) {
        const uint16_t mag = (pcm > 0x0FFF) ? 0x0FFFu : static_cast<uint16_t>(pcm);
        return s_alaw_encode_mag_table[mag];
    }

    int32_t mag32 = -(static_cast<int32_t>(pcm)) - 1;
    if (mag32 < 0) {
        mag32 = 0;
    } else if (mag32 > 0x0FFF) {
        mag32 = 0x0FFF;
    }
    return static_cast<uint8_t>(s_alaw_encode_mag_table[static_cast<uint16_t>(mag32)] ^ 0x80u);
}

static int16_t aLawToLinear(const uint8_t alaw) {
    if (!s_alaw_tables_ready) {
        initALawTables();
    }
    return s_alaw_decode_table[alaw];
}

static size_t buildNrlPacket(const uint8_t packet_type,
                             const uint8_t *payload,
                             const size_t payload_size,
                             uint8_t *out_packet,
                             const size_t out_capacity) {
    const size_t total_size = kNrlHeaderSize + payload_size;
    if (out_packet == nullptr || total_size > out_capacity) {
        return 0;
    }

    memset(out_packet, 0, total_size);
    memcpy(out_packet, "NRL2", 4u);
    out_packet[4] = static_cast<uint8_t>((total_size >> 8) & 0xFFu);
    out_packet[5] = static_cast<uint8_t>(total_size & 0xFFu);
    memcpy(out_packet + 6u, s_cpu_id, sizeof(s_cpu_id));
    out_packet[20] = packet_type;
    out_packet[21] = 1u;
    ++s_packet_counter;
    out_packet[22] = static_cast<uint8_t>((s_packet_counter >> 8) & 0xFFu);
    out_packet[23] = static_cast<uint8_t>(s_packet_counter & 0xFFu);
    strncpy(reinterpret_cast<char *>(out_packet + 24u), NRL_AUDIO_CALLSIGN, 6u);
    out_packet[30] = static_cast<uint8_t>(NRL_AUDIO_CALLSIGN_SSID);
    out_packet[31] = static_cast<uint8_t>(NRL_AUDIO_DEVICE_MODE);

    if (payload_size > 0u) {
        memcpy(out_packet + kNrlHeaderSize, payload, payload_size);
    }

    return total_size;
}

static bool parseNrlPacket(const uint8_t *packet,
                           const size_t packet_size,
                           uint8_t *out_type,
                           const uint8_t **out_payload,
                           size_t *out_payload_size) {
    if (packet == nullptr || packet_size < kNrlHeaderSize) {
        return false;
    }
    if (memcmp(packet, "NRL2", 4u) != 0) {
        return false;
    }

    const uint16_t total_size = static_cast<uint16_t>((packet[4] << 8) | packet[5]);
    if (total_size > packet_size || total_size < kNrlHeaderSize) {
        return false;
    }

    if (out_type != nullptr) {
        *out_type = packet[20];
    }
    if (out_payload != nullptr) {
        *out_payload = packet + kNrlHeaderSize;
    }
    if (out_payload_size != nullptr) {
        *out_payload_size = total_size - kNrlHeaderSize;
    }
    return true;
}

static bool udpSendPacket(const uint8_t *packet, const size_t packet_size) {
    if (packet == nullptr || packet_size == 0u || !s_udp_started || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    if (s_udp_mutex == nullptr) {
        return false;
    }
    if (xSemaphoreTake(s_udp_mutex, 0) != pdTRUE) {
        return false;
    }

    const bool ok = s_udp.beginPacket(s_server_ip, static_cast<uint16_t>(NRL_AUDIO_SERVER_PORT)) == 1 &&
                    s_udp.write(packet, packet_size) == packet_size &&
                    s_udp.endPacket() == 1;
    xSemaphoreGive(s_udp_mutex);
    return ok;
}

static void sendVoiceFrame(const int16_t *pcm8k, const size_t sample_count) {
    if (pcm8k == nullptr || sample_count == 0u) {
        return;
    }
    if (gCurrentFunction != FUNCTION_INCOMING && gCurrentFunction != FUNCTION_MONITOR) {
        return;
    }

    static uint8_t alaw_accumulator[kG711PayloadBytes];
    static size_t alaw_accumulator_count = 0u;

    const size_t used_samples = (sample_count < kAudioCodecFrameSamples) ? sample_count : kAudioCodecFrameSamples;
    for (size_t i = 0; i < used_samples; ++i) {
        if (alaw_accumulator_count < kG711PayloadBytes) {
            alaw_accumulator[alaw_accumulator_count++] = linearToALaw(pcm8k[i]);
        }

        if (alaw_accumulator_count == kG711PayloadBytes) {
            uint8_t packet[kNrlHeaderSize + kG711PayloadBytes];
            const size_t packet_size = buildNrlPacket(kNrlTypeVoice,
                                                      alaw_accumulator,
                                                      kG711PayloadBytes,
                                                      packet,
                                                      sizeof(packet));
            if (packet_size > 0u) {
                udpSendPacket(packet, packet_size);
            }
            alaw_accumulator_count = 0u;
        }
    }
}

static void es8311FrameHook(const int16_t *samples,
                            const size_t sample_count,
                            ES8311_AudioMode_t mode,
                            void *) {
    if (mode != ES8311_AUDIO_MODE_RECEIVE) {
        return;
    }
    sendVoiceFrame(samples, sample_count);
}

static bool ensureWifiAndUdp(void) {
    if (WiFi.status() != WL_CONNECTED) {
        const WifiConnResult result = wifiEnsureConnected(NRL_AUDIO_WIFI_SSID, NRL_AUDIO_WIFI_PASSWORD, 15000u);
        if (result != WIFI_CONN_OK && result != WIFI_ALREADY_ON_SSID) {
            return false;
        }
    }

    if (!s_udp_started) {
        if (!s_server_ip.fromString(NRL_AUDIO_SERVER_HOST)) {
            if (!WiFi.hostByName(NRL_AUDIO_SERVER_HOST, s_server_ip)) {
                Serial.printf("[NRL] resolve host failed: %s\n", NRL_AUDIO_SERVER_HOST);
                return false;
            }
        }
        s_udp_started = s_udp.begin(static_cast<uint16_t>(NRL_AUDIO_LOCAL_PORT)) == 1;
        if (!s_udp_started) {
            Serial.println("[NRL] udp begin failed");
            return false;
        }
        Serial.printf("[NRL] udp local=%u remote=%s(%s):%u\n",
                      static_cast<unsigned>(NRL_AUDIO_LOCAL_PORT),
                      NRL_AUDIO_SERVER_HOST,
                      s_server_ip.toString().c_str(),
                      static_cast<unsigned>(NRL_AUDIO_SERVER_PORT));
    }

    return true;
}

static void startBridgeTransmit(void) {
    if (s_bridge_tx_active) {
        return;
    }

    if (gCurrentFunction == FUNCTION_TRANSMIT) {
        return;
    }

    ES8311_ClearOutputQueue();
    RADIO_PrepareTX();
    if (gCurrentFunction == FUNCTION_TRANSMIT) {
        s_bridge_tx_active = true;
        AUDIO_AudioPathOn();
    }
}

static void stopBridgeTransmit(void) {
    if (!s_bridge_tx_active) {
        return;
    }

    ES8311_ClearOutputQueue();
    APP_EndTransmission(true);
    s_bridge_tx_active = false;
}

static void handleIncomingVoicePayload(const uint8_t *payload, const size_t payload_size) {
    if (payload == nullptr || payload_size < kG711RxPayloadMinBytes || payload_size > kG711RxPayloadMaxBytes) {
        return;
    }

    startBridgeTransmit();
    if (!s_bridge_tx_active) {
        return;
    }

    int16_t pcm8k[kG711RxPayloadMaxBytes];

    size_t offset = 0u;
    while (offset < payload_size) {
        const size_t chunk = ((payload_size - offset) < kG711RxPayloadMaxBytes)
                                 ? (payload_size - offset)
                                 : kG711RxPayloadMaxBytes;
        for (size_t i = 0; i < chunk; ++i) {
            pcm8k[i] = aLawToLinear(payload[offset + i]);
        }
        if (chunk > 0u) {
            ES8311_QueueOutputSamples(pcm8k, chunk);
        }
        offset += chunk;
    }

    s_last_rx_packet_ms = millis();
}

static void sendHeartbeat(void) {
    uint8_t packet[kNrlHeaderSize];
    const size_t packet_size = buildNrlPacket(kNrlTypeHeartbeat, nullptr, 0u, packet, sizeof(packet));
    if (packet_size > 0u) {
        udpSendPacket(packet, packet_size);
    }
}

static void bridgeTask(void *) {
    uint8_t packet_buffer[kNrlMaxPacketSize];

    while (true) {
        if (!ensureWifiAndUdp()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (s_udp_mutex != nullptr && xSemaphoreTake(s_udp_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            const int packet_size = s_udp.parsePacket();
            if (packet_size > 0) {
                const int clamped = (packet_size < static_cast<int>(sizeof(packet_buffer)))
                                        ? packet_size
                                        : static_cast<int>(sizeof(packet_buffer));
                const int bytes_read = s_udp.read(packet_buffer, clamped);
                if (bytes_read > 0) {
                    uint8_t packet_type = 0u;
                    const uint8_t *payload = nullptr;
                    size_t payload_size = 0u;
                    if (parseNrlPacket(packet_buffer,
                                       static_cast<size_t>(bytes_read),
                                       &packet_type,
                                       &payload,
                                       &payload_size) &&
                        (packet_type == kNrlTypeVoice || packet_type == kNrlTypeServerVoice)) {
                        updateRemoteIdentity(packet_buffer, static_cast<size_t>(bytes_read));
                        handleIncomingVoicePayload(payload, payload_size);
                    }
                }
            }
            xSemaphoreGive(s_udp_mutex);
        }

        const uint32_t now = millis();
        if (s_bridge_tx_active &&
            (now - s_last_rx_packet_ms) > static_cast<uint32_t>(NRL_AUDIO_RX_PACKET_TIMEOUT_MS)) {
            stopBridgeTransmit();
        }
        if ((now - s_last_heartbeat_ms) > static_cast<uint32_t>(NRL_AUDIO_HEARTBEAT_INTERVAL_MS)) {
            sendHeartbeat();
            s_last_heartbeat_ms = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

} // namespace

bool NRLAudioBridge_Init(void) {
    if (s_bridge_initialized) {
        return true;
    }

    initCpuId();
    initALawTables();

    if (s_udp_mutex == nullptr) {
        s_udp_mutex = xSemaphoreCreateMutex();
        if (s_udp_mutex == nullptr) {
            Serial.println("[NRL] failed to create udp mutex");
            return false;
        }
    }

    ES8311_SetFrameHook(es8311FrameHook, nullptr);

    if (xTaskCreate(bridgeTask, "nrl_audio_bridge", 6144, nullptr, 2, &s_bridge_task) != pdPASS) {
        Serial.println("[NRL] failed to create bridge task");
        return false;
    }

    s_bridge_initialized = true;
    Serial.println("[NRL] bridge task started");
    return true;
}

bool NRLAudioBridge_GetRemoteIdentity(char *buffer, size_t buffer_size) {
    if (buffer == nullptr || buffer_size == 0u) {
        return false;
    }

    const uint32_t now = millis();
    if (s_last_remote_identity_ms == 0u ||
        (now - s_last_remote_identity_ms) > static_cast<uint32_t>(NRL_AUDIO_RX_PACKET_TIMEOUT_MS + 2000u) ||
        s_remote_callsign[0] == '\0') {
        buffer[0] = '\0';
        return false;
    }

    snprintf(buffer, buffer_size, "%s-%u", s_remote_callsign, static_cast<unsigned>(s_remote_ssid));
    return true;
}
