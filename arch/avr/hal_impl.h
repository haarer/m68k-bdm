#ifndef HAL_IMPL_H
#define HAL_IMPL_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include "board_config.h"

/* ------------------------------------------------------------------ */
/*  AVR port number → register pointer mapping                         */
/* ------------------------------------------------------------------ */

#define AVR_PORTA 0
#define AVR_PORTB 1
#define AVR_PORTC 2
#define AVR_PORTD 3
#define AVR_PORTE 4
#define AVR_PORTF 5
#define AVR_PORTG 6
#define AVR_PORTH 7
#define AVR_PORTJ 8
#define AVR_PORTK 9
#define AVR_PORTL 10

static volatile uint8_t *avr_port_reg(uint8_t port)
{
    static volatile uint8_t *const ports[] = {
        &PORTA, &PORTB, &PORTC, &PORTD, &PORTE,
        &PORTF, &PORTG, &PORTH, &PORTJ, &PORTK, &PORTL
    };
    return ports[port];
}

static volatile uint8_t *avr_ddr_reg(uint8_t port)
{
    static volatile uint8_t *const ddrs[] = {
        &DDRA, &DDRB, &DDRC, &DDRD, &DDRE,
        &DDRF, &DDRG, &DDRH, &DDRJ, &DDRK, &DDRL
    };
    return ddrs[port];
}

static volatile uint8_t *avr_pin_reg(uint8_t port)
{
    static volatile uint8_t *const pins[] = {
        &PINA, &PINB, &PINC, &PIND, &PINE,
        &PINF, &PING, &PINH, &PINJ, &PINK, &PINL
    };
    return pins[port];
}

/* ------------------------------------------------------------------ */
/*  Interrupts                                                         */
/* ------------------------------------------------------------------ */

static inline void hal_impl_irq_enable(void)
{
    sei();
}

static inline void hal_impl_irq_disable(void)
{
    cli();
}

/* ------------------------------------------------------------------ */
/*  Delays                                                             */
/* ------------------------------------------------------------------ */

static inline void hal_impl_delay_us(uint16_t us)
{
    while (us--)
        _delay_us(1);
}

/* ------------------------------------------------------------------ */
/*  GPIO                                                               */
/* ------------------------------------------------------------------ */

static inline void hal_impl_gpio_set_output(uint8_t port, uint8_t pin)
{
    *avr_ddr_reg(port) |= (1 << pin);
}

static inline void hal_impl_gpio_set_input_pullup(uint8_t port, uint8_t pin)
{
    *avr_ddr_reg(port) &= ~(1 << pin);
    *avr_port_reg(port) |= (1 << pin);
}

static inline void hal_impl_gpio_set_high(uint8_t port, uint8_t pin)
{
    *avr_port_reg(port) |= (1 << pin);
}

static inline void hal_impl_gpio_set_low(uint8_t port, uint8_t pin)
{
    *avr_port_reg(port) &= ~(1 << pin);
}

static inline uint8_t hal_impl_gpio_read(uint8_t port, uint8_t pin)
{
    return (*avr_pin_reg(port) & (1 << pin)) ? 1 : 0;
}

#endif
