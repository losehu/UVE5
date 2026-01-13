/*
 * Minimal simavr core table for embedded builds.
 */
#ifndef __SIM_CORE_DECL_H__
#define __SIM_CORE_DECL_H__

#include "sim_core_config.h"

struct avr_kind_t;
typedef struct avr_kind_t avr_kind_t;

#if CONFIG_MEGA32U4
extern avr_kind_t mega32u4;
#endif

extern avr_kind_t *avr_kind[];

#ifdef AVR_KIND_DECL
avr_kind_t *avr_kind[] = {
#if CONFIG_MEGA32U4
    &mega32u4,
#endif
    NULL
};
#endif

#endif
