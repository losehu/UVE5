#include "shared_flash_c.h"

#if defined(ARDUINO_ARCH_ESP32) && !defined(ENABLE_OPENCV)
#include "../lib/shared_flash.h"

extern "C" bool shared_read_c(uint32_t offset, void *out, size_t len)
{
    return shared_read((size_t)offset, out, len);
}

extern "C" bool shared_write_c(uint32_t offset, const void *data, size_t len)
{
    return shared_write((size_t)offset, data, len);
}

#else

extern "C" bool shared_read_c(uint32_t, void *, size_t)
{
    return false;
}

extern "C" bool shared_write_c(uint32_t, const void *, size_t)
{
    return false;
}

#endif
