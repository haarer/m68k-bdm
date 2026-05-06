#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    uint8_t  buf[RX_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
} ringbuf_t;

void     ringbuf_init(ringbuf_t *rb);
bool     ringbuf_push(ringbuf_t *rb, uint8_t data);
bool     ringbuf_pop(ringbuf_t *rb, uint8_t *data);
uint16_t ringbuf_count(ringbuf_t *rb);
bool     ringbuf_is_empty(ringbuf_t *rb);
bool     ringbuf_is_full(ringbuf_t *rb);

#endif
