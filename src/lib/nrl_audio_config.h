#ifndef SRC_LIB_NRL_AUDIO_CONFIG_H
#define SRC_LIB_NRL_AUDIO_CONFIG_H

// Wi-Fi credentials used by the app-side network audio bridge.
#ifndef NRL_AUDIO_WIFI_SSID
#define NRL_AUDIO_WIFI_SSID "CVPU"
#endif

#ifndef NRL_AUDIO_WIFI_PASSWORD
#define NRL_AUDIO_WIFI_PASSWORD "CVPU123456"
#endif

// Remote NRL server endpoint.
#ifndef NRL_AUDIO_SERVER_HOST
#ifdef NRL_AUDIO_SERVER_IP
#define NRL_AUDIO_SERVER_HOST NRL_AUDIO_SERVER_IP
#else
#define NRL_AUDIO_SERVER_HOST "110.42.107.105"
#endif
#endif

#ifndef NRL_AUDIO_SERVER_PORT
#define NRL_AUDIO_SERVER_PORT 60050
#endif

#ifndef NRL_AUDIO_LOCAL_PORT
#define NRL_AUDIO_LOCAL_PORT 60050
#endif

// Basic NRL identity fields.
#ifndef NRL_AUDIO_CALLSIGN
#define NRL_AUDIO_CALLSIGN "NOCALL"
#endif

#ifndef NRL_AUDIO_CALLSIGN_SSID
#define NRL_AUDIO_CALLSIGN_SSID 0
#endif

#ifndef NRL_AUDIO_DEVICE_MODE
#define NRL_AUDIO_DEVICE_MODE  55
#endif

// Voice stream behaviour.
#ifndef NRL_AUDIO_RX_PACKET_TIMEOUT_MS
#define NRL_AUDIO_RX_PACKET_TIMEOUT_MS 250
#endif

#ifndef NRL_AUDIO_HEARTBEAT_INTERVAL_MS
#define NRL_AUDIO_HEARTBEAT_INTERVAL_MS 2000
#endif

#endif
