#ifdef ENABLE_ARDUBOY

#include "arduboy2.h"

#include <stdio.h>
#include <string.h>

#ifdef ENABLE_OPENCV
#include <chrono>
#else
#include <Arduino.h>
#endif

extern volatile bool gArduboyFrameReady;
extern "C" volatile bool gUpdateDisplay;

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

static uint8_t gArduboyPresent[2][Arduboy2::width * Arduboy2::height / 8];
static volatile uint8_t gArduboyPresentIndex = 0;

const uint8_t *Arduboy2_GetPresentBuffer()
{
    return gArduboyPresent[gArduboyPresentIndex];
}

void Arduboy2::begin() {
    clear();
    setFrameRate(ARDUBOY_DEFAULT_FPS);
    lastFrameMs = arduboy_millis();
    frameCount = 0;
    textSize = 1;
}

void Arduboy2::initRandomSeed() {
#ifdef ENABLE_OPENCV
    // Deterministic by default in simulator.
#else
    randomSeed(static_cast<uint32_t>(micros()) ^ static_cast<uint32_t>(millis()));
#endif
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
    ++frameCount;
    return true;
}

bool Arduboy2::everyXFrames(uint8_t frames) const {
    if (frames == 0) {
        return false;
    }
    return (frameCount % frames) == 0;
}

uint32_t Arduboy2::getMillis() const {
    return arduboy_millis();
}

void Arduboy2::pollButtons() {
    previousButtons = currentButtons;
    currentButtons = rawButtons;
    justPressedButtons = currentButtons & ~previousButtons;
    justReleasedButtons = ~currentButtons & previousButtons;
}

bool Arduboy2::pressed(uint8_t buttons) const {
    // Some upstream sketches call pressed() without ever calling pollButtons().
    // Use the raw button state so input still works; justPressed/justReleased
    // continue to require pollButtons() as usual.
    return (rawButtons & buttons) != 0;
}

bool Arduboy2::justPressed(uint8_t buttons) const {
    return (justPressedButtons & buttons) != 0;
}

bool Arduboy2::justReleased(uint8_t buttons) const {
    return (justReleasedButtons & buttons) != 0;
}

void Arduboy2::clear() {
    memset(buffer, 0, sizeof(buffer));
}

void Arduboy2::display() {
    // If the UI hasn't consumed the previous frame yet, avoid overwriting the
    // double-buffer slot it might still be copying from.
    // This prevents flicker/tearing when the game produces frames faster than
    // the UI can blit (common in very light scenes like intros).
    if (gArduboyFrameReady) {
        gUpdateDisplay = true;
        return;
    }

    const uint8_t next = static_cast<uint8_t>(gArduboyPresentIndex ^ 1U);
    memcpy(gArduboyPresent[next], buffer, sizeof(buffer));
    gArduboyPresentIndex = next;

    // Publish the new frame to the UI task.
    __sync_synchronize();
    gArduboyFrameReady = true;
    gUpdateDisplay = true;
}

void Arduboy2::display(bool clear_after) {
    // If the UI hasn't consumed the previous frame yet, don't publish a new
    // one and (critically) don't clear the buffer, otherwise we create visible
    // blank frames/flicker.
    if (gArduboyFrameReady) {
        gUpdateDisplay = true;
        return;
    }

    display();
    if (clear_after) {
        clear();
    }
}

void Arduboy2::digitalWriteRGB(uint8_t r, uint8_t g, uint8_t b) {
    (void)r;
    (void)g;
    (void)b;
}

static void DrawSpriteOverwrite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame, const uint8_t *mask, uint8_t mask_frame, bool self_masked) {
    if (!bitmap) {
        return;
    }
    const uint8_t w = pgm_read_byte(bitmap + 0);
    const uint8_t h = pgm_read_byte(bitmap + 1);
    if (w == 0 || h == 0) {
        return;
    }
    const uint16_t bytes_per_frame = static_cast<uint16_t>(w) * ((static_cast<uint16_t>(h) + 7) / 8);
    const uint8_t *bmp = bitmap + 2 + bytes_per_frame * frame;
    const uint8_t *msk = nullptr;
    if (mask) {
        const uint8_t mw = pgm_read_byte(mask + 0);
        const uint8_t mh = pgm_read_byte(mask + 1);
        if (mw == w && mh == h) {
            msk = mask + 2 + bytes_per_frame * mask_frame;
        }
    }

    for (uint8_t sx = 0; sx < w; ++sx) {
        for (uint8_t sy = 0; sy < h; ++sy) {
            const uint16_t bit_index = static_cast<uint16_t>(sx) + static_cast<uint16_t>(sy / 8) * w;
            const uint8_t bit_mask = static_cast<uint8_t>(1U << (sy & 7));
            const bool pixel_on = (pgm_read_byte(bmp + bit_index) & bit_mask) != 0;
            bool write = true;
            if (msk) {
                const bool mask_on = (pgm_read_byte(msk + bit_index) & bit_mask) != 0;
                write = mask_on;
            } else if (self_masked) {
                write = pixel_on;
            }

            if (!write) {
                continue;
            }

            if (self_masked) {
                arduboy.drawPixel(static_cast<int16_t>(x + sx), static_cast<int16_t>(y + sy), WHITE);
            } else {
                arduboy.drawPixel(static_cast<int16_t>(x + sx), static_cast<int16_t>(y + sy), pixel_on ? WHITE : BLACK);
            }
        }
    }
}

void Sprites::drawOverwrite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame) {
    DrawSpriteOverwrite(x, y, bitmap, frame, nullptr, 0, false);
}

void Sprites::drawSelfMasked(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame) {
    DrawSpriteOverwrite(x, y, bitmap, frame, nullptr, 0, true);
}

void Sprites::drawExternalMask(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t frame, uint8_t mask_frame) {
    DrawSpriteOverwrite(x, y, bitmap, frame, mask, mask_frame, false);
}

static void DrawSpritePlusMask(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame) {
    if (!bitmap) {
        return;
    }
    const uint8_t w = pgm_read_byte(bitmap + 0);
    const uint8_t h = pgm_read_byte(bitmap + 1);
    if (w == 0 || h == 0) {
        return;
    }

    const uint16_t bytes_per_frame = static_cast<uint16_t>(w) * ((static_cast<uint16_t>(h) + 7) / 8);
    // Plus-mask format stores image+mask interleaved: (img0, msk0, img1, msk1, ...)
    const uint8_t *data = bitmap + 2 + static_cast<uint32_t>(frame) * bytes_per_frame * 2U;

    for (uint8_t sx = 0; sx < w; ++sx) {
        for (uint8_t sy = 0; sy < h; ++sy) {
            const uint16_t bit_index = static_cast<uint16_t>(sx) + static_cast<uint16_t>(sy / 8) * w;
            const uint8_t bit_mask = static_cast<uint8_t>(1U << (sy & 7));

            const uint8_t img_byte = pgm_read_byte(data + bit_index * 2U);
            const uint8_t msk_byte = pgm_read_byte(data + bit_index * 2U + 1U);

            const bool mask_on = (msk_byte & bit_mask) != 0;
            if (!mask_on) {
                continue;
            }
            const bool pixel_on = (img_byte & bit_mask) != 0;
            arduboy.drawPixel(static_cast<int16_t>(x + sx), static_cast<int16_t>(y + sy), pixel_on ? WHITE : BLACK);
        }
    }
}

void Sprites::drawPlusMask(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame) {
    DrawSpritePlusMask(x, y, bitmap, frame);
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

void Arduboy2::setTextSize(uint8_t size) {
    if (size == 0) {
        size = 1;
    }
    // Keep it small; most sketches only use 1 or 2.
    if (size > 4) {
        size = 4;
    }
    textSize = size;
}

void Arduboy2::print(const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        const char c = *s++;
        if (c == '\n') {
            cursorX = 0;
            cursorY = static_cast<int16_t>(cursorY + (6 * (textSize ? textSize : 1)));
            continue;
        }
        if (c == '\r') {
            cursorX = 0;
            continue;
        }
        drawChar(cursorX, cursorY, c);
        cursorX = static_cast<int16_t>(cursorX + (4 * (textSize ? textSize : 1)));
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
    const uint8_t scale = textSize ? textSize : 1;
    for (uint8_t col = 0; col < 3; ++col) {
        uint8_t pixels = cols[col];
        for (uint8_t row = 0; row < 5; ++row) {
            if (pixels & 0x01) {
                const int16_t px = static_cast<int16_t>(x + static_cast<int16_t>(col * scale));
                const int16_t py = static_cast<int16_t>(y + static_cast<int16_t>(row * scale));
                for (uint8_t dx = 0; dx < scale; ++dx) {
                    for (uint8_t dy = 0; dy < scale; ++dy) {
                        drawPixel(static_cast<int16_t>(px + dx), static_cast<int16_t>(py + dy), WHITE);
                    }
                }
            }
            pixels >>= 1;
        }
    }
}

#endif // ENABLE_ARDUBOY
