#ifndef SRC_LIB_NRL_AUDIO_BRIDGE_H
#define SRC_LIB_NRL_AUDIO_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool NRLAudioBridge_Init(void);
bool NRLAudioBridge_GetRemoteIdentity(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif
