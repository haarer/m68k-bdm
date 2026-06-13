#pragma once
#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_putc_raw(char c);
void uart_write(const char *s, int len);
void uart_puts(const char *s);
int  uart_getc(void);
void uart_flush(void);
