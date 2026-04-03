#ifndef DRIVER_ES8311_H
#define DRIVER_ES8311_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ES8311_AUDIO_MODE_RECEIVE = 0,   // BK4819/BK1080 -> ES8311 -> speaker (switch LOW)
    ES8311_AUDIO_MODE_TRANSMIT = 1,  // mic -> ES8311 -> BK4819 input (switch HIGH)
} ES8311_AudioMode_t;

typedef void (*ES8311_FrameHook_t)(const int16_t *samples,
                                   size_t sample_count,
                                   ES8311_AudioMode_t mode,
                                   void *user_data);

// Initialize the ES8311 codec and start I2S pass-through.
bool ES8311_Init(void);

// True if the codec initialization sequence already succeeded.
bool ES8311_IsReady(void);

// Switch ES8311 external audio route.
bool ES8311_SetAudioMode(ES8311_AudioMode_t mode);
bool ES8311_SetReceiveMode(void);
bool ES8311_SetTransmitMode(void);

// Register a callback invoked from the ES8311 passthrough task.
// The hook is called for captured receive-mode PCM16 frames at 16 kHz.
void ES8311_SetFrameHook(ES8311_FrameHook_t hook, void *user_data);

// Queue PCM16 mono samples (16 kHz) for playback/transmit when in TX mode.
size_t ES8311_QueueOutputSamples(const int16_t *samples, size_t sample_count);
void ES8311_ClearOutputQueue(void);

int ES8311_GetSampleRate(void);
size_t ES8311_GetFrameSamples(void);

// Convenience test tone output to verify speaker chain.
bool ES8311_PlayTestTone(uint32_t durationMs);

// Record from mic for durationMs and then play back the captured audio once.
bool ES8311_RecordMicAndPlayback(uint32_t durationMs);

#ifdef __cplusplus
}
#endif

#endif // DRIVER_ES8311_H
