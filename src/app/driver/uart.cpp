/* ESP32-S3 UART0 driver using Arduino UART API
 * TX = U0TXD (GPIO43) / RX = U0RXD (GPIO44)
 */

#include "uart1.h"
#ifndef ENABLE_OPENCV
#include <Arduino.h>
#else
#include "../opencv/Arduino.hpp"
#endif
#define UART_NUM           0
#define UART_TX_PIN        43
#define UART_RX_PIN        44
#define UART_BUF_SIZE      1024

static uart_t* uart = NULL;
static bool UART_IsLogEnabled = true;

void UART_Init(uint32_t baud) {
    // Initialize UART0 using Arduino's uartBegin
    // uartBegin(uart_nr, baudrate, config, rxPin, txPin, rx_buffer_size, tx_buffer_size, inverted, rxfifo_full_thrhd)
    uart = uartBegin(UART_NUM, baud, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN, UART_BUF_SIZE, 0, false, 112);
}

void UART_Send(const void *pBuffer, uint32_t size) {
    if (uart) {
        const uint8_t *pData = (const uint8_t *)pBuffer;
        for (uint32_t i = 0; i < size; i++) {
            uartWrite(uart, pData[i]);
        }
    }
}

void UART_SetLogEnabled(bool enabled) {
    UART_IsLogEnabled = enabled;
}

void UART_LogSend(const void *pBuffer, uint32_t size) {
    if (!UART_IsLogEnabled) {
        return;
    }
    UART_Send(pBuffer, size);
}

int UART_Available(void) {
    return uart ? uartAvailable(uart) : 0;
}

int UART_Read(void) {
    if (uart && uartAvailable(uart) > 0) {
        return uartRead(uart);
    }
    return -1;
}
