/* ESP32 Keyboard Driver
 * Ported from UV-K5 firmware
 * 8x GPIO control 11 keys using matrix scanning
 * 
 * Hardware Configuration:
 * - KEY0-KEY3 (GPIO 48,21,18,17): Row inputs with pull-up
 * - KEY4-KEY7 (GPIO 14,13,12,11): Column outputs
 * - KEY4/KEY5 are shared with I2C (SDA/SCL)
 * - PTT, SIDE1, SIDE2: Active low buttons
 */

#include "keyboard.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>

// Global Variables
KEY_Code_t gKeyReading0 = KEY_INVALID;
KEY_Code_t gKeyReading1 = KEY_INVALID;
uint16_t gDebounceCounter = 0;
bool gWasFKeyPressed = false;

// Keyboard Matrix Configuration
static const struct {
    // Bitmask for column pins (KEY4-KEY7)
    gpio_num_t column_pin;  // Which column pin to set LOW
    
    // Row mappings (4 rows per column)
    struct {
        KEY_Code_t key;
        gpio_num_t row_pin;  // GPIO_KEY0 to GPIO_KEY3
    } rows[4];
    
} keyboard_matrix[] = {
    {
        // Column: All high (no column selected) - for SIDE keys
        .column_pin = (gpio_num_t)0xFF,  // Special value: don't pull any column low
        .rows = {
            {.key = KEY_SIDE1, .row_pin = GPIO_KEY0},
            {.key = KEY_SIDE2, .row_pin = GPIO_KEY1},
            {.key = KEY_INVALID, .row_pin = GPIO_KEY2},
            {.key = KEY_INVALID, .row_pin = GPIO_KEY3}
        }
    },
    {
        // Column 0: KEY4 (GPIO 14)
        .column_pin = GPIO_KEY4,
        .rows = {
            {.key = KEY_MENU, .row_pin = GPIO_KEY0},
            {.key = KEY_1,    .row_pin = GPIO_KEY1},
            {.key = KEY_4,    .row_pin = GPIO_KEY2},
            {.key = KEY_7,    .row_pin = GPIO_KEY3}
        }
    },
    {
        // Column 1: KEY5 (GPIO 13)
        .column_pin = GPIO_KEY5,
        .rows = {
            {.key = KEY_UP,   .row_pin = GPIO_KEY0},
            {.key = KEY_2,    .row_pin = GPIO_KEY1},
            {.key = KEY_5,    .row_pin = GPIO_KEY2},
            {.key = KEY_8,    .row_pin = GPIO_KEY3}
        }
    },
    {
        // Column 2: KEY6 (GPIO 12)
        .column_pin = GPIO_KEY6,
        .rows = {
            {.key = KEY_DOWN, .row_pin = GPIO_KEY0},
            {.key = KEY_3,    .row_pin = GPIO_KEY1},
            {.key = KEY_6,    .row_pin = GPIO_KEY2},
            {.key = KEY_9,    .row_pin = GPIO_KEY3}
        }
    },
    {
        // Column 3: KEY7 (GPIO 11)
        .column_pin = GPIO_KEY7,
        .rows = {
            {.key = KEY_EXIT, .row_pin = GPIO_KEY0},
            {.key = KEY_STAR, .row_pin = GPIO_KEY1},
            {.key = KEY_0,    .row_pin = GPIO_KEY2},
            {.key = KEY_F,    .row_pin = GPIO_KEY3}
        }
    }
};

// Initialize keyboard GPIOs
void KEYBOARD_Init(void) {
    // Configure row pins (KEY0-KEY3) as inputs with pull-up
    gpio_config_t row_config = {
        .pin_bit_mask = (1ULL << GPIO_KEY0) | (1ULL << GPIO_KEY1) | 
                        (1ULL << GPIO_KEY2) | (1ULL << GPIO_KEY3),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&row_config);
    
    // Configure column pins (KEY4-KEY7) as outputs, initially HIGH
    gpio_config_t col_config = {
        .pin_bit_mask = (1ULL << GPIO_KEY4) | (1ULL << GPIO_KEY5) | 
                        (1ULL << GPIO_KEY6) | (1ULL << GPIO_KEY7),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&col_config);
    
    // Set all column pins HIGH initially
    gpio_set_level(GPIO_KEY4, 1);
    gpio_set_level(GPIO_KEY5, 1);
    gpio_set_level(GPIO_KEY6, 1);
    gpio_set_level(GPIO_KEY7, 1);
    
    // Configure PTT button as input with pull-up (active low)
    gpio_config_t ptt_config = {
        .pin_bit_mask = (1ULL << GPIO_PTT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&ptt_config);
}

// Microsecond delay function
static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// Poll keyboard matrix
KEY_Code_t KEYBOARD_Poll(void) {
    KEY_Code_t Key = KEY_INVALID;
    
    // Scan through each column configuration
    for (unsigned int col = 0; col < sizeof(keyboard_matrix) / sizeof(keyboard_matrix[0]); col++) {
        // Set all column pins HIGH first
        gpio_set_level(GPIO_KEY4, 1);
        gpio_set_level(GPIO_KEY5, 1);
        gpio_set_level(GPIO_KEY6, 1);
        gpio_set_level(GPIO_KEY7, 1);
        
        // Set the target column LOW (or keep all high for SIDE keys)
        if (keyboard_matrix[col].column_pin != (gpio_num_t)0xFF) {
            gpio_set_level(keyboard_matrix[col].column_pin, 0);
        }
        
        // Small delay to let signals settle
        delay_us(10);
        
        // Read row pins with de-bounce (multiple samples)
        int stable_reads = 0;
        uint8_t last_state = 0;
        uint8_t current_state = 0;
        
        for (int sample = 0; sample < 8 && stable_reads < 3; sample++) {
            delay_us(1);
            
            // Read all 4 row pins
            current_state = 0;
            current_state |= (!gpio_get_level(GPIO_KEY0)) ? (1 << 0) : 0;
            current_state |= (!gpio_get_level(GPIO_KEY1)) ? (1 << 1) : 0;
            current_state |= (!gpio_get_level(GPIO_KEY2)) ? (1 << 2) : 0;
            current_state |= (!gpio_get_level(GPIO_KEY3)) ? (1 << 3) : 0;
            
            // Check if reading is stable
            if (current_state == last_state) {
                stable_reads++;
            } else {
                stable_reads = 0;
                last_state = current_state;
            }
        }
        
        // If noise is too bad, abort
        if (stable_reads < 3) {
            break;
        }
        
        // Check which row pin is pressed (active low)
        for (unsigned int row = 0; row < 4; row++) {
            if (current_state & (1 << row)) {
                Key = keyboard_matrix[col].rows[row].key;
                break;
            }
        }
        
        if (Key != KEY_INVALID) {
            break;
        }
    }
    
    // Restore I2C pins to HIGH (KEY4 and KEY5 are shared with I2C)
    // This is important to not interfere with I2C communication
    gpio_set_level(GPIO_KEY4, 1);
    gpio_set_level(GPIO_KEY5, 1);
    
    // Reset other column pins
    gpio_set_level(GPIO_KEY6, 0);
    gpio_set_level(GPIO_KEY7, 1);
    
    return Key;
}

// Get key including PTT button check
KEY_Code_t GetKey(void) {
    KEY_Code_t btn = KEYBOARD_Poll();
    
    // Check PTT button separately (active low)
    if (btn == KEY_INVALID && gpio_get_level(GPIO_PTT) == 0) {
        btn = KEY_PTT;
    }
    
    return btn;
}
