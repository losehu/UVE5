/* Minimal Arduboy2-compatible API for UVE5 */
#ifndef ARDUBOY2_H
#define ARDUBOY2_H

#include <stddef.h>
#include <stdint.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#endif

// Some sketches use `pgm_read_dword` to read pointers from PROGMEM.
// On 64-bit hosts (OpenCV build), return uintptr_t so pointer casts remain valid.
#ifndef pgm_read_dword
#if INTPTR_MAX > INT32_MAX
#define pgm_read_dword(addr) ((uintptr_t)(*(const void *const *)(addr)))
#else
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#endif
#endif

#ifndef strcpy_P
#define strcpy_P(dest, src) strcpy((dest), (src))
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

#ifndef ARDUBOY_DEFAULT_FPS
#define ARDUBOY_DEFAULT_FPS 60
#endif

#ifndef RGB_ON
#define RGB_ON 1
#endif
#ifndef RGB_OFF
#define RGB_OFF 0
#endif

// Minimal geometry helpers used by some Arduboy sketches.
struct Rect {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(int16_t x_, int16_t y_, int16_t w_, int16_t h_) : x(x_), y(y_), width(w_), height(h_) {}
};

class Arduboy2 {
public:
    static constexpr int16_t width = 128;
    static constexpr int16_t height = 64;

    struct Audio {
        bool enabled() const { return enabledFlag; }
        void toggle() { enabledFlag = !enabledFlag; }
        void on() { enabledFlag = true; }
        void off() { enabledFlag = false; }

        bool enabledFlag = true;
    } audio;

    void begin();
    void setFrameRate(uint8_t rate);
    bool nextFrame();
    bool everyXFrames(uint8_t frames) const;
    void initRandomSeed();

    uint32_t getMillis() const;

    void pollButtons();
    bool pressed(uint8_t buttons) const;
    bool justPressed(uint8_t buttons) const;
    bool justReleased(uint8_t buttons) const;

    void clear();
    void display();
    void display(bool clear);

    void drawPixel(int16_t x, int16_t y, uint8_t color = WHITE);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color = WHITE);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color = WHITE);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color = WHITE);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color = WHITE);
    void fillScreen(uint8_t color = BLACK) {
        if (color == BLACK) {
            clear();
        } else {
            for (size_t i = 0; i < sizeof(buffer); ++i) {
                buffer[i] = 0xFF;
            }
        }
    }
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color = WHITE);

    void setCursor(int16_t x, int16_t y);
    void setTextSize(uint8_t size);
    void print(const char *s);
    void println(const char *s) {
        print(s);
        println();
    }
    void println() {
        cursorX = 0;
        cursorY = static_cast<int16_t>(cursorY + (8 * (textSize ? textSize : 1)));
    }
    void print(int32_t value);
    void print(uint32_t value);

    void setButtonState(uint8_t state);
    void digitalWriteRGB(uint8_t r, uint8_t g, uint8_t b);
    void setRGBled(uint8_t r, uint8_t g, uint8_t b) { digitalWriteRGB(r, g, b); }

    bool nextFrameDEV() { return nextFrame(); }
    uint8_t cpuLoad() const { return 0; }

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
    uint32_t frameCount = 0;
    int16_t cursorX = 0;
    int16_t cursorY = 0;
    uint8_t textSize = 1;

    void drawChar(int16_t x, int16_t y, char c);

public:
    static bool collide(const Rect &a, const Rect &b) {
        return (a.x < (b.x + b.width)) &&
               ((a.x + a.width) > b.x) &&
               (a.y < (b.y + b.height)) &&
               ((a.y + a.height) > b.y);
    }
};

extern Arduboy2 arduboy;

// Returns a stable buffer containing the last frame passed to Arduboy2::display().
// This is useful when the game loop runs in a separate task.
const uint8_t *Arduboy2_GetPresentBuffer();

// Many sketches use Arduboy2Base; in this project it's the same shim.
using Arduboy2Base = Arduboy2;

#ifndef WIDTH
#define WIDTH Arduboy2::width
#endif
#ifndef HEIGHT
#define HEIGHT Arduboy2::height
#endif

#ifndef TONES_END
#define TONES_END 0x0000
#endif

class ArduboyTones {
public:
    explicit ArduboyTones(bool (*enabled_fn)()) : enabled(enabled_fn) {}

    void tones(const uint16_t * /*sequence*/) {
        if (enabled && !enabled()) {
            return;
        }
    }

private:
    bool (*enabled)();
};

class Sprites {
public:
    static void drawOverwrite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);
    static void drawSelfMasked(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);
    static void drawExternalMask(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t frame, uint8_t mask_frame);
    static void drawPlusMask(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);
};

#endif

