/*
 * sim_gdb.h
 *
 * Minimal GDB interface declarations (stubbed in embedded build).
 */
#ifndef __SIM_GDB_H__
#define __SIM_GDB_H__

#include <stdint.h>

struct avr_t;

#ifdef __cplusplus
extern "C" {
#endif

enum avr_gdb_watch_type {
    AVR_GDB_BREAK_SOFT   = 1 << 0,
    AVR_GDB_BREAK_HARD   = 1 << 1,
    AVR_GDB_WATCH_WRITE  = 1 << 2,
    AVR_GDB_WATCH_READ   = 1 << 3,
    AVR_GDB_WATCH_ACCESS = 1 << 4
};

int avr_gdb_init(avr_t *avr);
void avr_deinit_gdb(avr_t *avr);
int avr_gdb_processor(avr_t *avr, int sleep);
void avr_gdb_handle_watchpoints(avr_t *g, uint16_t addr, enum avr_gdb_watch_type type);
void avr_gdb_handle_break(avr_t *g);

#ifdef __cplusplus
}
#endif

#endif
