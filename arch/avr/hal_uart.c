#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "config.h"
#include "board_config.h"
#include "hal.h"
#include "ringbuf.h"

static ringbuf_t rx_buf;

void hal_serial_init(uint32_t baud)
{
    (void)baud;

    UCSR0A = (1 << U2X0);

    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    UBRR0H = (uint8_t)(SERIAL_UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)SERIAL_UBRR_VALUE;

    ringbuf_init(&rx_buf);
}

void hal_serial_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = c;
}

char hal_serial_getc(void)
{
    uint8_t data = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ringbuf_pop(&rx_buf, &data);
    }
    return (char)data;
}

int hal_serial_has_data(void)
{
    uint16_t count;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        count = ringbuf_count(&rx_buf);
    }
    return (int)count;
}

void hal_serial_flush(void)
{
    while (!(UCSR0A & (1 << TXC0)))
        ;
    UCSR0A |= (1 << TXC0);
}

void hal_serial_rx_isr_handler(uint8_t byte)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ringbuf_push(&rx_buf, byte);
    }
}

ISR(USART0_RX_vect)
{
    uint8_t data = UDR0;
    hal_serial_rx_isr_handler(data);
}

/* ------------------------------------------------------------------ */
/*  Timer — TCNT1 free-running at F_CPU (16-bit, wraps every ~4ms)    */
/*  Returns raw tick count; bdm_timing.c handles conversion          */
/* ------------------------------------------------------------------ */

void hal_timer_init(void)
{
    /* TCNT1 runs free at F_CPU with no prescaler (default after reset) */
}

uint32_t hal_timer_get_us(void)
{
    return (uint32_t)TCNT1;
}
