#include "arduboy2.h"

#include <stdio.h>
#include <string.h>

#ifdef ENABLE_OPENCV
#include <chrono>
#else
#include <Arduino.h>
#endif

extern bool gArduboyFrameReady;
extern "C" bool gUpdateDisplay;

extern "C" {
#include "../font.h"
}
#if ENABLE_CHINESE_FULL != 0 && !defined(ENABLE_ENGLISH)
extern "C" {
#include "../font_blob.h"
}
#endif

static uint32_t arduboy_millis(void) {
#ifdef ENABLE_OPENCV
    using namespace std::chrono;
    static const steady_clock::time_point start = steady_clock::now();
    return static_cast<uint32_t>(
        duration_cast<milliseconds>(steady_clock::now() - start).count());
#else
    return millis();
#endif
}

Arduboy2 arduboy;

void Arduboy2::begin() {
    clear();
    setFrameRate(30);
    lastFrameMs = arduboy_millis();
}

void Arduboy2::setFrameRate(uint8_t rate) {
    if (rate == 0) {
        rate = 30;
    }
    frameDurationMs = 1000U / rate;
    if (frameDurationMs == 0) {
        frameDurationMs = 1;
    }
}

bool Arduboy2::nextFrame() {
    const uint32_t now = arduboy_millis();
    if ((uint32_t)(now - lastFrameMs) < frameDurationMs) {
        return false;
    }
    lastFrameMs = now;
    return true;
}

void Arduboy2::pollButtons() {
    previousButtons = currentButtons;
    currentButtons = rawButtons;
    justPressedButtons = currentButtons & ~previousButtons;
    justReleasedButtons = ~currentButtons & previousButtons;
}

bool Arduboy2::pressed(uint8_t buttons) const {
    return (currentButtons & buttons) == buttons;
}

bool Arduboy2::justPressed(uint8_t buttons) const {
    return (justPressedButtons & buttons) == buttons;
}

bool Arduboy2::justReleased(uint8_t buttons) const {
    return (justReleasedButtons & buttons) == buttons;
}

void Arduboy2::clear() {
    memset(buffer, 0, sizeof(buffer));
}

void Arduboy2::display() {
    gArduboyFrameReady = true;
    gUpdateDisplay = true;
}

void Arduboy2::drawPixel(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    const uint16_t index = (static_cast<uint16_t>(y) >> 3) * width + static_cast<uint16_t>(x);
    const uint8_t mask = static_cast<uint8_t>(1U << (y & 7));
    if (color) {
        buffer[index] |= mask;
    } else {
        buffer[index] &= static_cast<uint8_t>(~mask);
    }
}

void Arduboy2::drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color) {
    for (int16_t i = 0; i < w; ++i) {
        drawPixel(static_cast<int16_t>(x + i), y, color);
    }
}

void Arduboy2::drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color) {
    for (int16_t i = 0; i < h; ++i) {
        drawPixel(x, static_cast<int16_t>(y + i), color);
    }
}

void Arduboy2::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, static_cast<int16_t>(y + h - 1), w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(static_cast<int16_t>(x + w - 1), y, h, color);
}

void Arduboy2::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    for (int16_t i = 0; i < h; ++i) {
        drawFastHLine(x, static_cast<int16_t>(y + i), w, color);
    }
}

void Arduboy2::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    for (int16_t j = 0; j < h; ++j) {
        const uint16_t row = static_cast<uint16_t>(j) >> 3;
        const uint8_t bit = static_cast<uint8_t>(1U << (j & 7));
        for (int16_t i = 0; i < w; ++i) {
            const uint16_t index = row * static_cast<uint16_t>(w) + static_cast<uint16_t>(i);
            const uint8_t val = pgm_read_byte(bitmap + index);
            if (val & bit) {
                drawPixel(static_cast<int16_t>(x + i), static_cast<int16_t>(y + j), color);
            }
        }
    }
}

void Arduboy2::setCursor(int16_t x, int16_t y) {
    cursorX = x;
    cursorY = y;
}

void Arduboy2::print(const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        const char c = *s++;
        if (c == '\n') {
            cursorX = 0;
            cursorY = static_cast<int16_t>(cursorY + 6);
            continue;
        }
        if (c == '\r') {
            cursorX = 0;
            continue;
        }
        drawChar(cursorX, cursorY, c);
        cursorX = static_cast<int16_t>(cursorX + 4);
    }
}

void Arduboy2::print(int32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", static_cast<long>(value));
    print(buf);
}

void Arduboy2::print(uint32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(value));
    print(buf);
}

void Arduboy2::setButtonState(uint8_t state) {
    rawButtons = state;
}

void Arduboy2::drawChar(int16_t x, int16_t y, char c) {
    uint8_t index = static_cast<uint8_t>(c);
    if (index < 0x20 || index > 0x7f) {
        index = '?';
    }
    index = static_cast<uint8_t>(index - 0x20);
    uint8_t glyph[3];
#if ENABLE_CHINESE_FULL != 0 && !defined(ENABLE_ENGLISH)
    FONT_Read(0x0255C + index * 3 - 0x02480, glyph, sizeof(glyph));
    const uint8_t *cols = glyph;
#else
    const uint8_t *cols = gFont3x5[index];
#endif
    for (uint8_t col = 0; col < 3; ++col) {
        uint8_t pixels = cols[col];
        for (uint8_t row = 0; row < 5; ++row) {
            if (pixels & 0x01) {
                drawPixel(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), WHITE);
            }
            pixels >>= 1;
        }
    }
}
