#include "checksum.h"

uint8_t checksum_xor(const uint8_t *data, size_t len)
{
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++)
        cs ^= data[i];
    return cs;
}

uint8_t checksum_sum(const uint8_t *data, size_t len)
{
    uint8_t cs = 0;
    for (size_t i = 0; i < len; i++)
        cs += data[i];
    return cs;
}
