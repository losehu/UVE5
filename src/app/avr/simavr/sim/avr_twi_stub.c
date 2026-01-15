#include "avr_twi.h"

#if defined(SIMAVR_STUB_TWI)
void avr_twi_init(avr_t *avr, avr_twi_t *port)
{
    (void)avr;
    (void)port;
}

uint32_t avr_twi_irq_msg(uint8_t msg, uint8_t addr, uint8_t data)
{
    (void)msg;
    (void)addr;
    (void)data;
    return 0;
}
#endif
