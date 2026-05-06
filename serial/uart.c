#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "config.h"
#include "uart.h"
#include "ringbuf.h"

static ringbuf_t rx_buf;

void uart_init(uint16_t ubrr)
{
    UCSR0A = (1 << U2X0);

    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;

    ringbuf_init(&rx_buf);
}

void uart_transmit(uint8_t data)
{
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = data;
}

bool uart_receive(uint8_t *data)
{
    bool result;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        result = ringbuf_pop(&rx_buf, data);
    }
    return result;
}

uint16_t uart_available(void)
{
    uint16_t count;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        count = ringbuf_count(&rx_buf);
    }
    return count;
}

void uart_send_buffer(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
        uart_transmit(buf[i]);
}

void uart_wait_transmit_complete(void)
{
    while (!(UCSR0A & (1 << TXC0)))
        ;
    UCSR0A |= (1 << TXC0);
}

ISR(USART0_RX_vect)
{
    uint8_t data = UDR0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ringbuf_push(&rx_buf, data);
    }
}
