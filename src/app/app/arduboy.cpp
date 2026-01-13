#include "arduboy.h"

#include "arduboy2.h"

#include <stdio.h>
#include <string.h>

extern "C" {
#include "../driver/st7565.h"
#include "../misc.h"
#include "../ui/helper.h"
#include "../ui/ui.h"
}

enum ArduboyMode {
    ARDUBOY_MODE_MENU = 0,
    ARDUBOY_MODE_GAME
};

struct ArduboyGame {
    const char *name;
    void (*init)(void);
    void (*loop)(void);
};

static ArduboyMode gArduboyMode = ARDUBOY_MODE_MENU;
static int gArduboySelected = 0;
static int gArduboyActive = -1;
static uint8_t gArduboyButtonState = 0;

bool gArduboyFrameReady = false;

static uint32_t gArduboyRng = 0x12345678U;

static uint32_t ArduboyRand(void) {
    gArduboyRng ^= gArduboyRng << 13;
    gArduboyRng ^= gArduboyRng >> 17;
    gArduboyRng ^= gArduboyRng << 5;
    return gArduboyRng;
}

static uint8_t ArduboyRandRange(uint8_t max) {
    if (max == 0) {
        return 0;
    }
    return static_cast<uint8_t>(ArduboyRand() % max);
}

static void ArduboySeed(void) {
    gArduboyRng ^= static_cast<uint32_t>(gFlashLightBlinkCounter);
}

static uint8_t ArduboyMapKey(KEY_Code_t key) {
    switch (key) {
        case KEY_UP:
        case KEY_2:
            return UP_BUTTON;
        case KEY_DOWN:
        case KEY_8:
            return DOWN_BUTTON;
        case KEY_4:
            return LEFT_BUTTON;
        case KEY_6:
            return RIGHT_BUTTON;
        case KEY_SIDE1:
        case KEY_MENU:
        case KEY_PTT:
            return A_BUTTON;
        case KEY_SIDE2:
            return B_BUTTON;
        default:
            return 0;
    }
}

static void ArduboySetButtons(uint8_t state) {
    gArduboyButtonState = state;
    arduboy.setButtonState(gArduboyButtonState);
}

static void ArduboyStartGame(int index);
static void ArduboyReturnToMenu(void);

// ----------------------- PONG -----------------------
static const int16_t PONG_PADDLE_X = 4;
static const int16_t PONG_PADDLE_H = 12;
static const int16_t PONG_BALL_SIZE = 2;

static int16_t pong_paddle_y;
static int16_t pong_ball_x;
static int16_t pong_ball_y;
static int8_t pong_ball_vx;
static int8_t pong_ball_vy;
static uint16_t pong_score;
static bool pong_waiting;

static void Pong_ResetBall(void) {
    pong_ball_x = Arduboy2::width / 2;
    pong_ball_y = Arduboy2::height / 2;
    pong_ball_vx = 1;
    pong_ball_vy = (ArduboyRand() & 1U) ? 1 : -1;
}

static void Pong_Init(void) {
    arduboy.setFrameRate(30);
    pong_score = 0;
    pong_paddle_y = (Arduboy2::height - PONG_PADDLE_H) / 2;
    pong_waiting = true;
    Pong_ResetBall();
}

static void Pong_Loop(void) {
    if (!arduboy.nextFrame()) {
        return;
    }
    arduboy.pollButtons();

    if (pong_waiting) {
        if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) {
            pong_waiting = false;
        }
    } else {
        if (arduboy.pressed(UP_BUTTON) && pong_paddle_y > 0) {
            pong_paddle_y -= 2;
        }
        if (arduboy.pressed(DOWN_BUTTON) &&
            pong_paddle_y < (Arduboy2::height - PONG_PADDLE_H)) {
            pong_paddle_y += 2;
        }

        pong_ball_x += pong_ball_vx;
        pong_ball_y += pong_ball_vy;

        if (pong_ball_y <= 0) {
            pong_ball_y = 0;
            pong_ball_vy = -pong_ball_vy;
        } else if (pong_ball_y >= (Arduboy2::height - PONG_BALL_SIZE)) {
            pong_ball_y = Arduboy2::height - PONG_BALL_SIZE;
            pong_ball_vy = -pong_ball_vy;
        }

        if (pong_ball_x >= (Arduboy2::width - PONG_BALL_SIZE)) {
            pong_ball_x = Arduboy2::width - PONG_BALL_SIZE;
            pong_ball_vx = -pong_ball_vx;
            pong_score++;
        }

        if (pong_ball_x <= (PONG_PADDLE_X + 1)) {
            if (pong_ball_y + PONG_BALL_SIZE >= pong_paddle_y &&
                pong_ball_y <= (pong_paddle_y + PONG_PADDLE_H)) {
                pong_ball_x = PONG_PADDLE_X + 2;
                pong_ball_vx = 1;
                const int16_t hit = pong_ball_y - pong_paddle_y;
                if (hit < (PONG_PADDLE_H / 3)) {
                    pong_ball_vy = -1;
                } else if (hit > ((PONG_PADDLE_H * 2) / 3)) {
                    pong_ball_vy = 1;
                }
            } else if (pong_ball_x < 0) {
                pong_score = 0;
                pong_waiting = true;
                Pong_ResetBall();
            }
        }
    }

    arduboy.clear();
    arduboy.drawFastVLine(PONG_PADDLE_X, pong_paddle_y, PONG_PADDLE_H, WHITE);
    arduboy.fillRect(pong_ball_x, pong_ball_y, PONG_BALL_SIZE, PONG_BALL_SIZE, WHITE);

    arduboy.setCursor(2, 0);
    arduboy.print("PONG");
    arduboy.setCursor(100, 0);
    arduboy.print(pong_score);

    if (pong_waiting) {
        arduboy.setCursor(28, 30);
        arduboy.print("PRESS A");
    }
    arduboy.display();
}

// ----------------------- SNAKE -----------------------
static const uint8_t SNAKE_CELL = 4;
static const uint8_t SNAKE_GRID_W = Arduboy2::width / SNAKE_CELL;
static const uint8_t SNAKE_GRID_H = Arduboy2::height / SNAKE_CELL;
static const uint8_t SNAKE_MAX_LEN = 96;

static uint8_t snake_x[SNAKE_MAX_LEN];
static uint8_t snake_y[SNAKE_MAX_LEN];
static uint8_t snake_len;
static uint8_t snake_dir;
static uint8_t snake_food_x;
static uint8_t snake_food_y;

static void Snake_SpawnFood(void) {
    while (true) {
        const uint8_t fx = ArduboyRandRange(SNAKE_GRID_W);
        const uint8_t fy = ArduboyRandRange(SNAKE_GRID_H);
        bool hit = false;
        for (uint8_t i = 0; i < snake_len; ++i) {
            if (snake_x[i] == fx && snake_y[i] == fy) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            snake_food_x = fx;
            snake_food_y = fy;
            return;
        }
    }
}

static void Snake_Reset(void) {
    snake_len = 4;
    snake_dir = 1;
    snake_x[0] = SNAKE_GRID_W / 2;
    snake_y[0] = SNAKE_GRID_H / 2;
    for (uint8_t i = 1; i < snake_len; ++i) {
        snake_x[i] = snake_x[0] - i;
        snake_y[i] = snake_y[0];
    }
    Snake_SpawnFood();
}

static void Snake_Init(void) {
    arduboy.setFrameRate(12);
    Snake_Reset();
}

static void Snake_Loop(void) {
    if (!arduboy.nextFrame()) {
        return;
    }
    arduboy.pollButtons();

    if (arduboy.justPressed(UP_BUTTON) && snake_dir != 2) {
        snake_dir = 0;
    } else if (arduboy.justPressed(RIGHT_BUTTON) && snake_dir != 3) {
        snake_dir = 1;
    } else if (arduboy.justPressed(DOWN_BUTTON) && snake_dir != 0) {
        snake_dir = 2;
    } else if (arduboy.justPressed(LEFT_BUTTON) && snake_dir != 1) {
        snake_dir = 3;
    }

    int16_t nx = snake_x[0];
    int16_t ny = snake_y[0];
    switch (snake_dir) {
        case 0: ny -= 1; break;
        case 1: nx += 1; break;
        case 2: ny += 1; break;
        case 3: nx -= 1; break;
        default: break;
    }

    if (nx < 0 || ny < 0 || nx >= SNAKE_GRID_W || ny >= SNAKE_GRID_H) {
        Snake_Reset();
        return;
    }
    for (uint8_t i = 0; i < snake_len; ++i) {
        if (snake_x[i] == nx && snake_y[i] == ny) {
            Snake_Reset();
            return;
        }
    }

    bool grow = (nx == snake_food_x && ny == snake_food_y);
    uint8_t new_len = snake_len;
    if (grow && snake_len < (SNAKE_MAX_LEN - 1)) {
        new_len = snake_len + 1;
    }

    for (int i = static_cast<int>(new_len) - 1; i > 0; --i) {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }
    snake_x[0] = static_cast<uint8_t>(nx);
    snake_y[0] = static_cast<uint8_t>(ny);
    snake_len = new_len;

    if (grow) {
        Snake_SpawnFood();
    }

    arduboy.clear();
    for (uint8_t i = 0; i < snake_len; ++i) {
        const int16_t px = static_cast<int16_t>(snake_x[i] * SNAKE_CELL);
        const int16_t py = static_cast<int16_t>(snake_y[i] * SNAKE_CELL);
        arduboy.fillRect(px, py, SNAKE_CELL, SNAKE_CELL, WHITE);
    }

    arduboy.drawRect(static_cast<int16_t>(snake_food_x * SNAKE_CELL),
                     static_cast<int16_t>(snake_food_y * SNAKE_CELL),
                     SNAKE_CELL, SNAKE_CELL, WHITE);

    arduboy.setCursor(0, 0);
    arduboy.print("LEN");
    arduboy.setCursor(14, 0);
    arduboy.print(static_cast<uint32_t>(snake_len));

    arduboy.display();
}

// --------------------- Game list --------------------
static ArduboyGame gArduboyGames[] = {
    {"PONG", &Pong_Init, &Pong_Loop},
    {"SNAKE", &Snake_Init, &Snake_Loop},
};

static const int gArduboyGameCount = sizeof(gArduboyGames) / sizeof(gArduboyGames[0]);

// --------------------- Menu/flow --------------------
static void ArduboyStartGame(int index) {
    if (index < 0 || index >= gArduboyGameCount) {
        return;
    }
    ArduboySeed();
    ArduboySetButtons(0);
    arduboy.begin();
    gArduboyActive = index;
    gArduboyMode = ARDUBOY_MODE_GAME;
    gArduboyGames[index].init();
    gArduboyFrameReady = false;
    arduboy.display();
}

static void ArduboyReturnToMenu(void) {
    ArduboySetButtons(0);
    gArduboyActive = -1;
    gArduboyMode = ARDUBOY_MODE_MENU;
    gUpdateDisplay = true;
}

void ARDUBOY_Enter(void) {
    gArduboyMode = ARDUBOY_MODE_MENU;
    gArduboyActive = -1;
    gArduboySelected = 0;
    ArduboySetButtons(0);
    gArduboyFrameReady = false;
    gRequestDisplayScreen = DISPLAY_ARDUBOY;
    gUpdateDisplay = true;
}

void ARDUBOY_ExitToMain(void) {
    ArduboySetButtons(0);
    gArduboyActive = -1;
    gArduboyMode = ARDUBOY_MODE_MENU;
    gRequestDisplayScreen = DISPLAY_MAIN;
    gUpdateDisplay = true;
}

void ARDUBOY_TimeSlice10ms(void) {
    if (gArduboyMode == ARDUBOY_MODE_GAME && gArduboyActive >= 0) {
        gArduboyGames[gArduboyActive].loop();
    }
}

void ARDUBOY_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
    if (gArduboyMode == ARDUBOY_MODE_MENU) {
        if (bKeyPressed) {
            return;
        }
        if (Key == KEY_UP || Key == KEY_2) {
            gArduboySelected--;
            if (gArduboySelected < 0) {
                gArduboySelected = gArduboyGameCount - 1;
            }
            gUpdateDisplay = true;
            return;
        }
        if (Key == KEY_DOWN || Key == KEY_8) {
            gArduboySelected++;
            if (gArduboySelected >= gArduboyGameCount) {
                gArduboySelected = 0;
            }
            gUpdateDisplay = true;
            return;
        }
        if (Key == KEY_MENU || Key == KEY_SIDE1 || Key == KEY_PTT) {
            ArduboyStartGame(gArduboySelected);
            return;
        }
        if (Key == KEY_EXIT) {
            ARDUBOY_ExitToMain();
            return;
        }
        return;
    }

    if (Key == KEY_EXIT && !bKeyPressed) {
        ArduboyReturnToMenu();
        return;
    }

    const uint8_t mask = ArduboyMapKey(Key);
    if (mask == 0) {
        return;
    }

    if (bKeyPressed) {
        ArduboySetButtons(static_cast<uint8_t>(gArduboyButtonState | mask));
    } else {
        ArduboySetButtons(static_cast<uint8_t>(gArduboyButtonState & ~mask));
    }

    (void)bKeyHeld;
}

static void ArduboyRenderMenu(void) {
    memset(gStatusLine, 0, sizeof(gStatusLine));
    UI_DisplayClear();
    UI_PrintStringSmall("Arduboy2", 0, 127, 0);

    for (int i = 0; i < gArduboyGameCount && i < 5; ++i) {
        char line[16];
        snprintf(line, sizeof(line), "%c%s", (i == gArduboySelected) ? '>' : ' ', gArduboyGames[i].name);
        UI_PrintStringSmall(line, 0, 127, static_cast<uint8_t>(i + 1));
    }

    UI_PrintStringSmall("EXIT=BACK", 0, 127, 6);
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
}

static void ArduboyRenderGame(void) {
    const uint8_t *src = arduboy.getBuffer();
    memcpy(gStatusLine, src, LCD_WIDTH);
    for (int i = 0; i < FRAME_LINES; ++i) {
        memcpy(gFrameBuffer[i], src + (LCD_WIDTH * (i + 1)), LCD_WIDTH);
    }
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();
    gArduboyFrameReady = false;
}

void ARDUBOY_Render(void) {
    if (gArduboyMode == ARDUBOY_MODE_MENU) {
        ArduboyRenderMenu();
        return;
    }

    if (!gArduboyFrameReady) {
        return;
    }
    ArduboyRenderGame();
}
