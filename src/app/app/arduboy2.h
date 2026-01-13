/* Minimal Arduboy2-compatible API for UVE5 */
#ifndef ARDUBOY2_H
#define ARDUBOY2_H

#include <stddef.h>
#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

#ifndef WHITE
#define WHITE 1
#endif
#ifndef BLACK
#define BLACK 0
#endif

#define LEFT_BUTTON  (1U << 0)
#define RIGHT_BUTTON (1U << 1)
#define UP_BUTTON    (1U << 2)
#define DOWN_BUTTON  (1U << 3)
#define A_BUTTON     (1U << 4)
#define B_BUTTON     (1U << 5)

class Arduboy2 {
public:
    static constexpr int16_t width = 128;
    static constexpr int16_t height = 64;

    void begin();
    void setFrameRate(uint8_t rate);
    bool nextFrame();

    void pollButtons();
    bool pressed(uint8_t buttons) const;
    bool justPressed(uint8_t buttons) const;
    bool justReleased(uint8_t buttons) const;

    void clear();
    void display();

    void drawPixel(int16_t x, int16_t y, uint8_t color = WHITE);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color = WHITE);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color = WHITE);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color = WHITE);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color = WHITE);
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color = WHITE);

    void setCursor(int16_t x, int16_t y);
    void print(const char *s);
    void print(int32_t value);
    void print(uint32_t value);

    void setButtonState(uint8_t state);

    const uint8_t *getBuffer() const { return buffer; }
    uint8_t *getBuffer() { return buffer; }

private:
    uint8_t buffer[width * height / 8];
    uint8_t rawButtons = 0;
    uint8_t currentButtons = 0;
    uint8_t previousButtons = 0;
    uint8_t justPressedButtons = 0;
    uint8_t justReleasedButtons = 0;
    uint32_t frameDurationMs = 33;
    uint32_t lastFrameMs = 0;
    int16_t cursorX = 0;
    int16_t cursorY = 0;

    void drawChar(int16_t x, int16_t y, char c);
};

extern Arduboy2 arduboy;

#endif
