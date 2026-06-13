#include <stdint.h>
#include <stddef.h>

struct Ringbuffer {
    uint8_t  buf[256];
    volatile uint16_t head;
    volatile uint16_t tail;
};

inline uint16_t ringbuffer_avail(const struct Ringbuffer *rb) { return rb->head - rb->tail; }
inline uint16_t ringbuffer_free(const struct Ringbuffer *rb)  { return sizeof(rb->buf) - (rb->head - rb->tail) - 1; }
inline int      ringbuffer_empty(const struct Ringbuffer *rb) { return rb->head == rb->tail; }
inline int      ringbuffer_full(const struct Ringbuffer *rb)  { return sizeof(rb->buf) - 1 < (uint16_t)(rb->head - rb->tail); }
inline void     ringbuffer_clear(struct Ringbuffer *rb)       { rb->head = rb->tail; }
inline void     ringbuffer_put_head(struct Ringbuffer *rb, uint8_t c) { rb->buf[rb->head++ % sizeof rb->buf] = c; }
inline uint8_t  ringbuffer_get_tail(struct Ringbuffer *rb)               { return rb->buf[rb->tail++ % sizeof rb->buf]; }


