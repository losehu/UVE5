#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// C-callable wrappers for ESP32 "shared" partition access.
// Return true on success.
bool shared_read_c(uint32_t offset, void *out, size_t len);
bool shared_write_c(uint32_t offset, const void *data, size_t len);

// Returns the size (bytes) of the ESP32 "shared" partition.
// Returns 0 if the partition can't be found.
uint32_t shared_size_c(void);

#ifdef __cplusplus
}
#endif
