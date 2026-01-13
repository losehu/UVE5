#include "arduboy_avr.h"

#include "arduboy2.h"
#include "arduboy_avr_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../driver/st7565.h"
#include "../misc.h"
#include "../ui/helper.h"
#include "../ui/ui.h"
}

extern "C" {
#include "sim_avr.h"
#include "sim_hex.h"
#include "sim_io.h"
#include "sim_time.h"
#include "avr_ioport.h"
#include "ssd1306_virt.h"
}

static avr_t *gArduboyAvr = NULL;
static ssd1306_t gArduboyDisplay;
static avr_irq_t *gPortBIn = NULL;
static avr_irq_t *gPortEIn = NULL;
static avr_irq_t *gPortFIn = NULL;

static bool gArduboyAvrInitialized = false;
static bool gArduboyAvrFrameReady = false;
static bool gArduboyAvrHasFrame = false;
static uint8_t gArduboyAvrButtons = 0;

enum ArduboyAvrMode {
    ARDUBOY_AVR_MODE_MENU = 0,
    ARDUBOY_AVR_MODE_GAME
};

static ArduboyAvrMode gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
static int gArduboyAvrSelected = 0;

static void ArduboyAvrStartGame(int index);

static void ArduboyAvrClampSelection(void) {
    if (gArduboyAvrRomCount == 0) {
        gArduboyAvrSelected = 0;
        return;
    }
    if (gArduboyAvrSelected < 0) {
        gArduboyAvrSelected = 0;
    }
    const int max_index = static_cast<int>(gArduboyAvrRomCount) - 1;
    if (gArduboyAvrSelected > max_index) {
        gArduboyAvrSelected = max_index;
    }
}

static void ArduboyAvrSleep(avr_t *avr, avr_cycle_count_t howLong) {
    (void)avr;
    (void)howLong;
}

static uint8_t ArduboyAvrReverseBits(uint8_t v) {
    v = static_cast<uint8_t>((v >> 4) | (v << 4));
    v = static_cast<uint8_t>(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
    v = static_cast<uint8_t>(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
    return v;
}

static void ArduboyAvrUpdateButtons(void) {
    if (!gArduboyAvr) {
        return;
    }

    uint8_t portf = 0xFF;
    uint8_t porte = 0xFF;
    uint8_t portb = 0xFF;

    if (gArduboyAvrButtons & LEFT_BUTTON) {
        portf &= static_cast<uint8_t>(~(1U << 5));
    }
    if (gArduboyAvrButtons & RIGHT_BUTTON) {
        portf &= static_cast<uint8_t>(~(1U << 6));
    }
    if (gArduboyAvrButtons & UP_BUTTON) {
        portf &= static_cast<uint8_t>(~(1U << 7));
    }
    if (gArduboyAvrButtons & DOWN_BUTTON) {
        portf &= static_cast<uint8_t>(~(1U << 4));
    }
    if (gArduboyAvrButtons & A_BUTTON) {
        porte &= static_cast<uint8_t>(~(1U << 6));
    }
    if (gArduboyAvrButtons & B_BUTTON) {
        portb &= static_cast<uint8_t>(~(1U << 4));
    }

    if (gPortFIn) {
        avr_raise_irq(gPortFIn, portf);
    }
    if (gPortEIn) {
        avr_raise_irq(gPortEIn, porte);
    }
    if (gPortBIn) {
        avr_raise_irq(gPortBIn, portb);
    }
}

static bool ArduboyAvrLoadHex(avr_t *avr, const char *hex) {
    if (!avr || !hex) {
        return false;
    }

    uint32_t segment = 0;
    const char *line = hex;
    char line_buf[560];
    uint8_t bline[272];

    while (*line) {
        const char *eol = strchr(line, '\n');
        size_t len = eol ? static_cast<size_t>(eol - line) : strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            len--;
        }
        if (len == 0) {
            line = eol ? eol + 1 : line + len;
            continue;
        }
        if (line[0] != ':') {
            line = eol ? eol + 1 : line + len;
            continue;
        }
        if (len - 1 >= sizeof(line_buf)) {
            return false;
        }
        memcpy(line_buf, line + 1, len - 1);
        line_buf[len - 1] = '\0';

        const int hexlen = read_hex_string(line_buf, bline, sizeof(bline));
        if (hexlen < 5) {
            return false;
        }

        uint8_t chk = 0;
        for (int i = 0; i < hexlen - 1; ++i) {
            chk = static_cast<uint8_t>(chk + bline[i]);
        }
        chk = static_cast<uint8_t>(0x100 - chk);
        if (chk != bline[hexlen - 1]) {
            return false;
        }

        const uint8_t count = bline[0];
        const uint16_t addr = static_cast<uint16_t>(bline[1] << 8) | bline[2];
        const uint8_t type = bline[3];
        if (hexlen < static_cast<int>(count + 5)) {
            return false;
        }

        switch (type) {
            case 0x00: {
                if (count > 0) {
                    const uint32_t load_addr = segment | addr;
                    avr_loadcode(avr, bline + 4, count, load_addr);
                }
                break;
            }
            case 0x01:
                return true;
            case 0x02: {
                if (count >= 2) {
                    segment = (static_cast<uint32_t>(bline[4]) << 8 | bline[5]) << 4;
                }
                break;
            }
            case 0x04: {
                if (count >= 2) {
                    segment = (static_cast<uint32_t>(bline[4]) << 8 | bline[5]) << 16;
                }
                break;
            }
            default:
                break;
        }

        line = eol ? eol + 1 : line + len;
    }
    return true;
}

static bool ArduboyAvrInit(void) {
    if (gArduboyAvrInitialized) {
        return true;
    }

    gArduboyAvr = avr_make_mcu_by_name("atmega32u4");
    if (!gArduboyAvr) {
        return false;
    }
    if (avr_init(gArduboyAvr) != 0) {
        free(gArduboyAvr);
        gArduboyAvr = NULL;
        return false;
    }

    gArduboyAvr->frequency = 16000000;
    gArduboyAvr->sleep = ArduboyAvrSleep;
    gArduboyAvr->log = LOG_ERROR;

    ssd1306_init(gArduboyAvr, &gArduboyDisplay, SSD1306_VIRT_COLUMNS,
                 SSD1306_VIRT_PAGES * 8);
    ssd1306_wiring_t wiring = {
        .chip_select = { 'D', 6 },
        .data_instruction = { 'D', 4 },
        .reset = { 'D', 7 },
    };
    ssd1306_connect(&gArduboyDisplay, &wiring);

    gPortBIn = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('B'),
                             IOPORT_IRQ_PIN_ALL_IN);
    gPortEIn = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('E'),
                             IOPORT_IRQ_PIN_ALL_IN);
    gPortFIn = avr_io_getirq(gArduboyAvr, AVR_IOCTL_IOPORT_GETIRQ('F'),
                             IOPORT_IRQ_PIN_ALL_IN);
    if (!gPortBIn || !gPortEIn || !gPortFIn) {
        avr_terminate(gArduboyAvr);
        free(gArduboyAvr);
        gArduboyAvr = NULL;
        return false;
    }

    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    avr_reset(gArduboyAvr);
    gArduboyAvr->run_cycle_limit = 4000;
    ssd1306_set_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY, 1);

    gArduboyAvrInitialized = true;
    return true;
}

void ARDUBOY_AVR_Enter(void) {
    if (!ArduboyAvrInit()) {
        return;
    }

    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
    ArduboyAvrClampSelection();

    gArduboyAvrHasFrame = false;
    gArduboyAvrFrameReady = false;
    gRequestDisplayScreen = DISPLAY_ARDUBOY_AVR;
    GUI_SelectNextDisplay(DISPLAY_ARDUBOY_AVR);
    gRequestDisplayScreen = DISPLAY_INVALID;
    gUpdateDisplay = true;
}

void ARDUBOY_AVR_ExitToMain(void) {
    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    gArduboyAvrMode = ARDUBOY_AVR_MODE_MENU;
    gRequestDisplayScreen = DISPLAY_MAIN;
    gUpdateDisplay = true;
}

void ARDUBOY_AVR_TimeSlice10ms(void) {
    if (!gArduboyAvrInitialized) {
        return;
    }
    if (gArduboyAvrMode != ARDUBOY_AVR_MODE_GAME) {
        return;
    }

    const avr_cycle_count_t target =
        gArduboyAvr->cycle + avr_usec_to_cycles(gArduboyAvr, 10000);

    while (gArduboyAvr->state == cpu_Running && gArduboyAvr->cycle < target) {
        avr_run(gArduboyAvr);
    }

    if (ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY)) {
        gArduboyAvrFrameReady = true;
        gUpdateDisplay = true;
    }
}

static uint8_t ArduboyAvrMapKey(KEY_Code_t key) {
    switch (key) {
        case KEY_2:
            return UP_BUTTON;
        case KEY_8:
            return DOWN_BUTTON;
        case KEY_4:
            return LEFT_BUTTON;
        case KEY_6:
            return RIGHT_BUTTON;
        case KEY_UP:
            return A_BUTTON;
        case KEY_DOWN:
            return B_BUTTON;
        default:
            return 0;
    }
}

void ARDUBOY_AVR_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (Key == KEY_EXIT && !bKeyPressed) {
        ARDUBOY_AVR_ExitToMain();
        return;
    }

    if (gArduboyAvrMode == ARDUBOY_AVR_MODE_MENU) {
        if (!bKeyPressed || bKeyHeld) {
            return;
        }
        if (Key == KEY_2) {
            gArduboyAvrSelected--;
            ArduboyAvrClampSelection();
            gUpdateDisplay = true;
            return;
        }
        if (Key == KEY_8) {
            gArduboyAvrSelected++;
            ArduboyAvrClampSelection();
            gUpdateDisplay = true;
            return;
        }
        if (Key == KEY_UP) {
            ArduboyAvrStartGame(gArduboyAvrSelected);
            return;
        }
        return;
    }

    const uint8_t mask = ArduboyAvrMapKey(Key);
    if (mask == 0) {
        return;
    }

    if (bKeyPressed) {
        gArduboyAvrButtons = static_cast<uint8_t>(gArduboyAvrButtons | mask);
    } else {
        gArduboyAvrButtons = static_cast<uint8_t>(gArduboyAvrButtons & ~mask);
    }
    ArduboyAvrUpdateButtons();

    (void)bKeyHeld;
}

static void ArduboyAvrRenderBlank(void) {
    memset(gStatusLine, 0, sizeof(gStatusLine));
    for (int i = 0; i < FRAME_LINES; ++i) {
        memset(gFrameBuffer[i], 0, sizeof(gFrameBuffer[i]));
    }
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
}

static void ArduboyAvrRenderMenu(void) {
    memset(gStatusLine, 0, sizeof(gStatusLine));
    UI_DisplayClear();
    UI_PrintStringSmall("Arduboy AVR", 0, 127, 0);

    if (gArduboyAvrRomCount == 0) {
        UI_PrintStringSmall("No ROMs", 0, 127, 2);
        UI_PrintStringSmall("EXIT=BACK", 0, 127, 6);
        ST7565_BlitStatusLine();
        ST7565_BlitFullScreen();
        return;
    }

    const int total = static_cast<int>(gArduboyAvrRomCount);
    const int window = 5;
    int start = 0;
    if (total > window) {
        start = gArduboyAvrSelected - window / 2;
        if (start < 0) {
            start = 0;
        }
        if (start > total - window) {
            start = total - window;
        }
    }

    const int lines = (total < window) ? total : window;
    for (int i = 0; i < lines; ++i) {
        const int index = start + i;
        char line[20];
        snprintf(line, sizeof(line), "%c%s",
                 (index == gArduboyAvrSelected) ? '>' : ' ',
                 gArduboyAvrRoms[index].name);
        UI_PrintStringSmall(line, 0, 127, static_cast<uint8_t>(i + 1));
    }

    UI_PrintStringSmall("EXIT=BACK", 0, 127, 6);
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
}

static void ArduboyAvrStartGame(int index) {
    if (gArduboyAvrRomCount == 0) {
        return;
    }
    if (index < 0 || index >= static_cast<int>(gArduboyAvrRomCount)) {
        return;
    }
    if (!ArduboyAvrInit()) {
        return;
    }

    gArduboyAvrButtons = 0;
    ArduboyAvrUpdateButtons();
    if (!ArduboyAvrLoadHex(gArduboyAvr, gArduboyAvrRoms[index].hex)) {
        return;
    }

    avr_reset(gArduboyAvr);
    gArduboyAvr->run_cycle_limit = 4000;
    ssd1306_set_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY, 1);

    gArduboyAvrMode = ARDUBOY_AVR_MODE_GAME;
    gArduboyAvrHasFrame = false;
    gArduboyAvrFrameReady = false;
    gUpdateDisplay = true;
}

static void ArduboyAvrRenderDisplay(void) {
    const bool display_on = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_DISPLAY_ON);
    if (!display_on) {
        ArduboyAvrRenderBlank();
        return;
    }

    const bool flip_x = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_SEGMENT_REMAP_0);
    const bool flip_y = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_COM_SCAN_NORMAL);
    const bool invert = ssd1306_get_flag(&gArduboyDisplay, SSD1306_FLAG_DISPLAY_INVERTED);

    for (int page = 0; page < SSD1306_VIRT_PAGES; ++page) {
        const int src_page = flip_y ? (SSD1306_VIRT_PAGES - 1 - page) : page;
        uint8_t *dest = (page == 0) ? gStatusLine : gFrameBuffer[page - 1];
        for (int col = 0; col < SSD1306_VIRT_COLUMNS; ++col) {
            const int src_col = flip_x ? (SSD1306_VIRT_COLUMNS - 1 - col) : col;
            uint8_t v = gArduboyDisplay.vram[src_page][src_col];
            if (flip_y) {
                v = ArduboyAvrReverseBits(v);
            }
            if (invert) {
                v = static_cast<uint8_t>(~v);
            }
            dest[col] = v;
        }
    }

    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
}

void ARDUBOY_AVR_Render(void) {
    if (!gArduboyAvrInitialized) {
        return;
    }

    if (gArduboyAvrMode == ARDUBOY_AVR_MODE_MENU) {
        ArduboyAvrRenderMenu();
        return;
    }

    if (!gArduboyAvrFrameReady) {
        if (!gArduboyAvrHasFrame) {
            ArduboyAvrRenderBlank();
            gArduboyAvrHasFrame = true;
        }
        return;
    }

    ArduboyAvrRenderDisplay();
    gArduboyAvrFrameReady = false;
    gArduboyAvrHasFrame = true;
    ssd1306_set_flag(&gArduboyDisplay, SSD1306_FLAG_DIRTY, 0);
}
