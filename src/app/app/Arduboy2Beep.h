// Minimal Arduboy2Beep compatibility shim for integrated sketches.
#ifndef ARDUBOY2_BEEP_H
#define ARDUBOY2_BEEP_H

#include <stdint.h>

class BeepPin1 {
public:
    void begin() {}
    void timer() {}

    uint16_t freq(uint16_t pitch) const { return pitch; }
    void tone(uint16_t /*frequency*/, uint8_t /*duration*/) {}
};

#endif
