#pragma once
#include "stdint.h"
#include "stdbool.h"
#ifdef __cplusplus
#include <iostream>
#endif
#include "HardwareSerial.h"
#if (CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)
#define NUM_OUPUT_PINS  46
#define PIN_DAC1        17
#define PIN_DAC2        18
#else
#define NUM_OUPUT_PINS  34
#define PIN_DAC1        25
#define PIN_DAC2        26
#endif

#define LOW               0x0
#define HIGH              0x1

//GPIO FUNCTIONS
#define INPUT             0x01
// Changed OUTPUT from 0x02 to behave the same as Arduino pinMode(pin,OUTPUT) 
// where you can read the state of pin even when it is set as OUTPUT
#define OUTPUT            0x03 
#define PULLUP            0x04
#define INPUT_PULLUP      0x05
#define PULLDOWN          0x08
#define INPUT_PULLDOWN    0x09
#define OPEN_DRAIN        0x10
#define OUTPUT_OPEN_DRAIN 0x13
#define ANALOG            0xC0

//Interrupt Modes
#define DISABLED  0x00
#define RISING    0x01
#define FALLING   0x02
#define CHANGE    0x03
#define ONLOW     0x04
#define ONHIGH    0x05
#define ONLOW_WE  0x0C
#define ONHIGH_WE 0x0D

typedef enum {
    GPIO_NUM_NC = -1,    /*!< Use to signal not connected to S/W */
    GPIO_NUM_0 = 0,     /*!< GPIO0, input and output */
    GPIO_NUM_1 = 1,     /*!< GPIO1, input and output */
    GPIO_NUM_2 = 2,     /*!< GPIO2, input and output */
    GPIO_NUM_3 = 3,     /*!< GPIO3, input and output */
    GPIO_NUM_4 = 4,     /*!< GPIO4, input and output */
    GPIO_NUM_5 = 5,     /*!< GPIO5, input and output */
    GPIO_NUM_6 = 6,     /*!< GPIO6, input and output */
    GPIO_NUM_7 = 7,     /*!< GPIO7, input and output */
    GPIO_NUM_8 = 8,     /*!< GPIO8, input and output */
    GPIO_NUM_9 = 9,     /*!< GPIO9, input and output */
    GPIO_NUM_10 = 10,   /*!< GPIO10, input and output */
    GPIO_NUM_11 = 11,   /*!< GPIO11, input and output */
    GPIO_NUM_12 = 12,   /*!< GPIO12, input and output */
    GPIO_NUM_13 = 13,   /*!< GPIO13, input and output */
    GPIO_NUM_14 = 14,   /*!< GPIO14, input and output */
    GPIO_NUM_15 = 15,   /*!< GPIO15, input and output */
    GPIO_NUM_16 = 16,   /*!< GPIO16, input and output */
    GPIO_NUM_17 = 17,   /*!< GPIO17, input and output */
    GPIO_NUM_18 = 18,   /*!< GPIO18, input and output */
    GPIO_NUM_19 = 19,   /*!< GPIO19, input and output */
    GPIO_NUM_20 = 20,   /*!< GPIO20, input and output */
    GPIO_NUM_21 = 21,   /*!< GPIO21, input and output */
    GPIO_NUM_26 = 26,   /*!< GPIO26, input and output */
    GPIO_NUM_27 = 27,   /*!< GPIO27, input and output */
    GPIO_NUM_28 = 28,   /*!< GPIO28, input and output */
    GPIO_NUM_29 = 29,   /*!< GPIO29, input and output */
    GPIO_NUM_30 = 30,   /*!< GPIO30, input and output */
    GPIO_NUM_31 = 31,   /*!< GPIO31, input and output */
    GPIO_NUM_32 = 32,   /*!< GPIO32, input and output */
    GPIO_NUM_33 = 33,   /*!< GPIO33, input and output */
    GPIO_NUM_34 = 34,   /*!< GPIO34, input and output */
    GPIO_NUM_35 = 35,   /*!< GPIO35, input and output */
    GPIO_NUM_36 = 36,   /*!< GPIO36, input and output */
    GPIO_NUM_37 = 37,   /*!< GPIO37, input and output */
    GPIO_NUM_38 = 38,   /*!< GPIO38, input and output */
    GPIO_NUM_39 = 39,   /*!< GPIO39, input and output */
    GPIO_NUM_40 = 40,   /*!< GPIO40, input and output */
    GPIO_NUM_41 = 41,   /*!< GPIO41, input and output */
    GPIO_NUM_42 = 42,   /*!< GPIO42, input and output */
    GPIO_NUM_43 = 43,   /*!< GPIO43, input and output */
    GPIO_NUM_44 = 44,   /*!< GPIO44, input and output */
    GPIO_NUM_45 = 45,   /*!< GPIO45, input and output */
    GPIO_NUM_46 = 46,   /*!< GPIO46, input and output */
    GPIO_NUM_47 = 47,   /*!< GPIO47, input and output */
    GPIO_NUM_48 = 48,   /*!< GPIO48, input and output */
    GPIO_NUM_MAX,
/** @endcond */
} gpio_num_t;
typedef enum {
    GPIO_MODE_DISABLE = 0,                                                         /*!< GPIO mode : disable input and output             */
    GPIO_MODE_INPUT = 1,                                                             /*!< GPIO mode : input only                           */
    GPIO_MODE_OUTPUT =2 ,                                                           /*!< GPIO mode : output only mode                     */
    GPIO_MODE_OUTPUT_OD = 3,                               /*!< GPIO mode : output only with open-drain mode     */
    GPIO_MODE_INPUT_OUTPUT_OD =4, /*!< GPIO mode : output and input with open-drain mode*/
    GPIO_MODE_INPUT_OUTPUT = 5,                         /*!< GPIO mode : output and input mode                */
} gpio_mode_t;
typedef enum {
    GPIO_PULLUP_DISABLE = 0x0,     /*!< Disable GPIO pull-up resistor */
    GPIO_PULLUP_ENABLE = 0x1,      /*!< Enable GPIO pull-up resistor */
} gpio_pullup_t;

typedef enum {
    GPIO_PULLDOWN_DISABLE = 0x0,   /*!< Disable GPIO pull-down resistor */
    GPIO_PULLDOWN_ENABLE = 0x1,    /*!< Enable GPIO pull-down resistor  */
} gpio_pulldown_t;

typedef enum {
    GPIO_INTR_DISABLE = 0,     /*!< Disable GPIO interrupt                             */
    GPIO_INTR_POSEDGE = 1,     /*!< GPIO interrupt type : rising edge                  */
    GPIO_INTR_NEGEDGE = 2,     /*!< GPIO interrupt type : falling edge                 */
    GPIO_INTR_ANYEDGE = 3,     /*!< GPIO interrupt type : both rising and falling edge */
    GPIO_INTR_LOW_LEVEL = 4,   /*!< GPIO interrupt type : input low level trigger      */
    GPIO_INTR_HIGH_LEVEL = 5,  /*!< GPIO interrupt type : input high level trigger     */
    GPIO_INTR_MAX,
} gpio_int_type_t;
typedef struct {
    uint64_t pin_bit_mask;          /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
    gpio_mode_t mode;               /*!< GPIO mode: set input/output mode                     */
    gpio_pullup_t pull_up_en;       /*!< GPIO pull-up                                         */
    gpio_pulldown_t pull_down_en;   /*!< GPIO pull-down                                       */
    gpio_int_type_t intr_type;      /*!< GPIO interrupt type                                  */
} gpio_config_t;
typedef int esp_err_t;
#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gpio_config(const gpio_config_t *pGPIOConfig);
esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode);
struct QueueDefinition; /* Using old naming convention so as not to break kernel aware debuggers. */
typedef struct QueueDefinition   * QueueHandle_t;
struct uart_struct_t {



    uint8_t num;                               // UART number for IDF driver API
    bool has_peek;                             // flag to indicate that there is a peek byte pending to be read
    uint8_t peek_byte;                         // peek byte that has been read but not consumed
    QueueHandle_t uart_event_queue;            // export it by some uartGetEventQueue() function
    // configuration data:: Arduino API tipical data
    int8_t _rxPin, _txPin, _ctsPin, _rtsPin;   // UART GPIOs
    uint32_t _baudrate, _config;               // UART baudrate and config
    // UART ESP32 specific data
    uint16_t _rx_buffer_size, _tx_buffer_size; // UART RX and TX buffer sizes
    bool _inverted;                            // UART inverted signal
    uint8_t _rxfifo_full_thrhd;                // UART RX FIFO full threshold
};



typedef struct uart_struct_t uart_t;
uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint32_t rx_buffer_size, uint32_t tx_buffer_size, bool inverted, uint8_t rxfifo_full_thrhd);

void esp_restart();

void digitalWrite(uint8_t pin, uint8_t val);
void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);


void uartWrite(uart_t* uart, uint8_t c);


uint32_t uartAvailable(uart_t* uart);
uint8_t uartRead(uart_t* uart);
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
void OPENCV_ShutdownDisplay(void);
void OPENCV_SetRestartArgs(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct EspRestartException {};
#endif

#ifdef __cplusplus
  #include <chrono>
  #include <thread>

  static inline void delay(unsigned ms) {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }

  static inline void delayMicroseconds(unsigned us) {
      std::this_thread::sleep_for(std::chrono::microseconds(us));
  }

#else
  #include <unistd.h>

  static inline void delay(unsigned ms) {
      usleep((useconds_t)ms * 1000);
  }

  static inline void delayMicroseconds(unsigned us) {
      usleep((useconds_t)us);
  }
#endif
#define ets_delay_us(us) delayMicroseconds(us)
#define esp_rom_delay_us(us) delayMicroseconds(us)

#ifdef __cplusplus
extern "C" {
#endif

int gpio_get_level(gpio_num_t gpio_num);
void noInterrupts(void);
void interrupts(void);
void analogWrite(uint8_t pin, int value) ;
uint16_t analogRead(uint8_t pin);
void analogReadResolution(uint8_t bits);
void switch_to_factory_and_restart() ;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
  // C++ only

class HWCDC
{
private:
    static void deinit();
    static bool isCDC_Connected();

public:
    HWCDC();
    ~HWCDC();



    size_t setRxBufferSize(size_t);
    size_t setTxBufferSize(size_t);
    void setTxTimeoutMs(uint32_t timeout);
    void begin(unsigned long baud=0);
    void end();
    
    int available(void);
    int availableForWrite(void);
    int peek(void);
    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    void flush(void);
    
    static bool isPlugged(void);
    inline static bool isConnected(void)
    {
        return isCDC_Connected();
    }
    inline size_t read(char * buffer, size_t size)
    {
        return 0;
    }
    inline size_t write(const char * buffer, size_t size)
    {
        return 0;
    }
    inline size_t write(const char * s)
    {
        return 0;
    }
    inline size_t write(unsigned long n)
    {
        return 0;
    }
    inline size_t write(long n)
    {
        return 0;
    }
    inline size_t write(unsigned int n)
    {
        return 0;
    }
    inline size_t write(int n)
    {
        return 0;
    }
    inline size_t println(char *a)
    {
        std::cout<<a<<std::endl;
        return 0;
    }
    operator bool() const;
    void setDebugOutput(bool);
    uint32_t baudRate(){return 115200;}

};
extern HWCDC Serial;
#endif
