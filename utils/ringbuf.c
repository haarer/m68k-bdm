#include "ringbuf.h"

void ringbuf_init(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

bool ringbuf_push(ringbuf_t *rb, uint8_t data)
{
    uint16_t next = (rb->head + 1) % RX_BUFFER_SIZE;
    if (next == rb->tail)
        return false;

    rb->buf[rb->head] = data;
    rb->head = next;
    return true;
}

bool ringbuf_pop(ringbuf_t *rb, uint8_t *data)
{
    if (rb->head == rb->tail)
        return false;

    *data = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % RX_BUFFER_SIZE;
    return true;
}

uint16_t ringbuf_count(ringbuf_t *rb)
{
    if (rb->head >= rb->tail)
        return rb->head - rb->tail;
    return RX_BUFFER_SIZE - rb->tail + rb->head;
}

bool ringbuf_is_empty(ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

bool ringbuf_is_full(ringbuf_t *rb)
{
    uint16_t next = (rb->head + 1) % RX_BUFFER_SIZE;
    return next == rb->tail;
}
