#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Simple UART API (ESP32-S3)
// Initialize UART0 with default pins (U0TXD/U0RXD)
void UART_Init(uint32_t baud);

// Send raw buffer
void UART_Send(const void *pBuffer, uint32_t size);

// Enable/disable log sending
void UART_SetLogEnabled(bool enabled);

// Send only when log enabled
void UART_LogSend(const void *pBuffer, uint32_t size);

// Get number of bytes available to read
int UART_Available(void);

// Read one byte from UART
int UART_Read(void);

#ifdef __cplusplus
}
#endif

