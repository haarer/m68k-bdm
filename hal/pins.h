#ifndef PINS_H
#define PINS_H

#include <stdbool.h>
#include <stdint.h>
#include "hal.h"

/* ------------------------------------------------------------------ */
/*  BDM pin identifiers — abstract port/pin handles                    */
/*  Defined per-architecture in board_config.h                         */
/* ------------------------------------------------------------------ */

/* Pin operations using the HAL GPIO abstraction */
static inline void pin_set_output(uint8_t port, uint8_t pin)
{
    hal_gpio_set_output(port, pin);
}

static inline void pin_set(uint8_t port, uint8_t pin)
{
    hal_gpio_set_high(port, pin);
}

static inline void pin_clear(uint8_t port, uint8_t pin)
{
    hal_gpio_set_low(port, pin);
}

static inline bool pin_read(uint8_t port, uint8_t pin)
{
    return hal_gpio_read(port, pin) != 0;
}

#endif
