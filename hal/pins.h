#ifndef PINS_H
#define PINS_H

#include <stdbool.h>
#include <stdint.h>

void     pin_init_output(volatile uint8_t *ddr, volatile uint8_t *port, uint8_t bit);
void     pin_init_input(volatile uint8_t *ddr, volatile uint8_t *port, uint8_t bit);
void     pin_set(volatile uint8_t *port, uint8_t bit);
void     pin_clear(volatile uint8_t *port, uint8_t bit);
void     pin_toggle(volatile uint8_t *port, uint8_t bit);
bool     pin_read(volatile uint8_t *pin_reg, uint8_t bit);
void     pin_set_mode(volatile uint8_t *ddr, uint8_t bit, bool output);

#endif
