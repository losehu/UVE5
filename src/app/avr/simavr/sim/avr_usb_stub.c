#include "avr_usb.h"

#if defined(SIMAVR_STUB_USB)
void avr_usb_init(avr_t *avr, avr_usb_t *port)
{
    (void)avr;
    (void)port;
}
#endif

