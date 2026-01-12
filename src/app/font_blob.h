/* Embedded font blob access. */
#ifndef FONT_BLOB_H
#define FONT_BLOB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const unsigned int gFontBlobSize;
extern const unsigned char gFontBlob[];
void FONT_Read(uint32_t address, void *buffer, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif
