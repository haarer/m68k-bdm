#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void     uart_init(uint16_t ubrr);
void     uart_transmit(uint8_t data);
bool     uart_receive(uint8_t *data);
uint16_t uart_available(void);
void     uart_send_buffer(const uint8_t *buf, size_t len);
void     uart_wait_transmit_complete(void);

#endif
