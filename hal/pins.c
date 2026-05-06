#include "pins.h"

void pin_init_output(volatile uint8_t *ddr, volatile uint8_t *port, uint8_t bit)
{
    *ddr |=  (1 << bit);
    *port &= ~(1 << bit);
}

void pin_init_input(volatile uint8_t *ddr, volatile uint8_t *port, uint8_t bit)
{
    *ddr &= ~(1 << bit);
    *port &= ~(1 << bit);
}

void pin_set(volatile uint8_t *port, uint8_t bit)
{
    *port |= (1 << bit);
}

void pin_clear(volatile uint8_t *port, uint8_t bit)
{
    *port &= ~(1 << bit);
}

void pin_toggle(volatile uint8_t *port, uint8_t bit)
{
    *port ^= (1 << bit);
}

bool pin_read(volatile uint8_t *pin_reg, uint8_t bit)
{
    return (*pin_reg & (1 << bit)) != 0;
}

void pin_set_mode(volatile uint8_t *ddr, uint8_t bit, bool output)
{
    if (output)
        *ddr |= (1 << bit);
    else
        *ddr &= ~(1 << bit);
}
