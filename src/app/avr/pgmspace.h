// Compatibility shim for sketches that include <avr/pgmspace.h>
// (UVE5 builds run on ESP32, not AVR).
#ifndef AVR_PGMSPACE_H
#define AVR_PGMSPACE_H

#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#endif

#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#endif

#endif

