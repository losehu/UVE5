#include "avr_uart.h"

#if defined(SIMAVR_STUB_UART)
void avr_uart_init(avr_t *avr, avr_uart_t *port)
{
    (void)avr;
    (void)port;
}
#endif

