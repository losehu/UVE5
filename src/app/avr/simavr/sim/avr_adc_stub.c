#include "avr_adc.h"

#if defined(SIMAVR_STUB_ADC)
void avr_adc_init(avr_t *avr, avr_adc_t *port)
{
    (void)avr;
    (void)port;
}
#endif

