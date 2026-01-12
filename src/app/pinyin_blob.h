/* Embedded pinyin blob access. */
#ifndef PINYIN_BLOB_H
#define PINYIN_BLOB_H

#include <stdint.h>

#define PINYIN_BLOB_BASE 0x20000u

#ifdef __cplusplus
extern "C" {
#endif

extern const unsigned int gPinyinBlobSize;
extern const unsigned char gPinyinBlob[];
void PINYIN_Read(uint32_t address, void *buffer, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif
