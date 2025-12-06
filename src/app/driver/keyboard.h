/* ESP32 Keyboard Driver
 * Ported from UV-K5 firmware
 * 8x GPIO control 11 keys using matrix scanning
 */

#ifndef ESP32_KEYBOARD_H
#define ESP32_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPIO Pin Definitions
// Row pins (inputs with pull-up)
#define GPIO_KEY0   ((gpio_num_t)48)  // Row 0 (SIDE1, SIDE2 when all columns high)
#define GPIO_KEY1   ((gpio_num_t)21)  // Row 1
#define GPIO_KEY2   ((gpio_num_t)18)  // Row 2
#define GPIO_KEY3   ((gpio_num_t)17)  // Row 3

// Column pins (outputs)
#define GPIO_KEY4   ((gpio_num_t)14)  // Column 0 (Shared with I2C SDA)
#define GPIO_KEY5   ((gpio_num_t)13)  // Column 1 (Shared with I2C SCL)
#define GPIO_KEY6   ((gpio_num_t)12)  // Column 2
#define GPIO_KEY7   ((gpio_num_t)11)  // Column 3

// Independent button pins (active low)
// Note: These need to be defined based on your hardware
// Assuming PTT might be on a separate GPIO
#define GPIO_PTT    ((gpio_num_t)0)  // PTT button (active low)

// Key Code Enumeration
enum KEY_Code_e {
    KEY_0 = 0,  // 0
    KEY_1,      // 1
    KEY_2,      // 2
    KEY_3,      // 3
    KEY_4,      // 4
    KEY_5,      // 5
    KEY_6,      // 6
    KEY_7,      // 7
    KEY_8,      // 8
    KEY_9,      // 9
    KEY_MENU,   // A
    KEY_UP,     // B
    KEY_DOWN,   // C
    KEY_EXIT,   // D
    KEY_STAR,   // *
    KEY_F,      // #
    KEY_PTT,    //
    KEY_SIDE2,  //
    KEY_SIDE1,  //
    KEY_INVALID //
};
typedef enum KEY_Code_e KEY_Code_t;

// Global Variables
extern KEY_Code_t gKeyReading0;
extern KEY_Code_t gKeyReading1;
extern uint16_t   gDebounceCounter;
extern bool       gWasFKeyPressed;

// Function Prototypes
void KEYBOARD_Init(void);
KEY_Code_t KEYBOARD_Poll(void);
KEY_Code_t GetKey(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_KEYBOARD_H
