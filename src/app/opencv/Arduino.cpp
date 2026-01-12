#include "Arduino.hpp"
#include "../scheduler.h"

void digitalWrite(uint8_t pin, uint8_t val)
{
    (void)pin;
    (void)val;
}

void esp_restart()
{
}

void pinMode(uint8_t pin, uint8_t mode)
{
    (void)pin;
    (void)mode;
}

int digitalRead(uint8_t pin)
{
    (void)pin;
    return HIGH;
}

void SCHEDULER_Init(void)
{
}

esp_err_t gpio_config(const gpio_config_t *pGPIOConfig)
{
    (void)pGPIOConfig;
    return 0;
}

esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode)
{
    (void)gpio_num;
    (void)mode;
    return 0;
}

uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint32_t rx_buffer_size, uint32_t tx_buffer_size, bool inverted, uint8_t rxfifo_full_thrhd)
{
    static uart_t uart_instances[3];
    uint8_t index = uart_nr;

    if (index >= (sizeof(uart_instances) / sizeof(uart_instances[0]))) {
        index = 0;
    }

    uart_t *uart = &uart_instances[index];
    uart->num = uart_nr;
    uart->has_peek = false;
    uart->peek_byte = 0;
    uart->uart_event_queue = nullptr;
    uart->_rxPin = rxPin;
    uart->_txPin = txPin;
    uart->_ctsPin = -1;
    uart->_rtsPin = -1;
    uart->_baudrate = baudrate;
    uart->_config = config;
    uart->_rx_buffer_size = rx_buffer_size;
    uart->_tx_buffer_size = tx_buffer_size;
    uart->_inverted = inverted;
    uart->_rxfifo_full_thrhd = rxfifo_full_thrhd;
    return uart;
}

void uartWrite(uart_t* uart, uint8_t c)
{
    (void)uart;
    (void)c;
}

uint32_t uartAvailable(uart_t* uart)
{
    (void)uart;
    return 0;
}

uint8_t uartRead(uart_t* uart)
{
    (void)uart;
    return 0;
}

esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    (void)gpio_num;
    (void)level;
    return 0;
}

int gpio_get_level(gpio_num_t gpio_num)
{
    (void)gpio_num;
    return 1;
}

void noInterrupts(void)
{
}

void interrupts(void)
{
}

void analogWrite(uint8_t pin, int value)
{
    (void)pin;
    (void)value;
}

uint16_t analogRead(uint8_t pin)
{
    (void)pin;
    return 0;
}

void analogReadResolution(uint8_t bits)
{
    (void)bits;
}

void switch_to_factory_and_restart()
{
}

HWCDC::HWCDC() = default;
HWCDC::~HWCDC() = default;

void HWCDC::deinit()
{
}

bool HWCDC::isCDC_Connected()
{
    return false;
}

size_t HWCDC::setRxBufferSize(size_t size)
{
    return size;
}

size_t HWCDC::setTxBufferSize(size_t size)
{
    return size;
}

void HWCDC::setTxTimeoutMs(uint32_t timeout)
{
    (void)timeout;
}

void HWCDC::begin(unsigned long baud)
{
    (void)baud;
}

void HWCDC::end()
{
}

int HWCDC::available(void)
{
    return 0;
}

int HWCDC::availableForWrite(void)
{
    return 0;
}

int HWCDC::peek(void)
{
    return -1;
}

int HWCDC::read(void)
{
    return -1;
}

size_t HWCDC::read(uint8_t *buffer, size_t size)
{
    (void)buffer;
    (void)size;
    return 0;
}

size_t HWCDC::write(uint8_t)
{
    return 1;
}

size_t HWCDC::write(const uint8_t *buffer, size_t size)
{
    (void)buffer;
    return size;
}

void HWCDC::flush(void)
{
}

bool HWCDC::isPlugged(void)
{
    return false;
}

HWCDC::operator bool() const
{
    return true;
}

void HWCDC::setDebugOutput(bool)
{
}

HWCDC Serial;
