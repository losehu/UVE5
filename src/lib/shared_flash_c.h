#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// C-callable wrappers for shared partition access (ESP32 only).
// Return true on success.
bool shared_read_c(uint32_t offset, void *out, size_t len);
bool shared_write_c(uint32_t offset, const void *data, size_t len);

#ifdef __cplusplus
} // extern "C"
#endif
