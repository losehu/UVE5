/*
 * Minimal ELF stubs for embedded simavr usage.
 */
#include "sim_elf.h"

int elf_read_firmware(const char *file, elf_firmware_t *firmware) {
    (void)file;
    (void)firmware;
    return -1;
}

void avr_load_firmware(avr_t *avr, elf_firmware_t *firmware) {
    (void)avr;
    (void)firmware;
}

int avr_read_dwarf(avr_t *avr, const char *filename) {
    (void)avr;
    (void)filename;
    return -1;
}
