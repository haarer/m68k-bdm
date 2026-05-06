#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

uint8_t checksum_xor(const uint8_t *data, size_t len);
uint8_t checksum_sum(const uint8_t *data, size_t len);

#endif
