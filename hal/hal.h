#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  Include architecture-specific inline implementations               */
/* ------------------------------------------------------------------ */
#include "hal_impl.h"

/* ------------------------------------------------------------------ */
/*  Interrupts                                                         */
/* ------------------------------------------------------------------ */

static inline void hal_irq_enable(void)
{
    hal_impl_irq_enable();
}

static inline void hal_irq_disable(void)
{
    hal_impl_irq_disable();
}

/* ------------------------------------------------------------------ */
/*  Delays                                                             */
/* ------------------------------------------------------------------ */

static inline void hal_delay_us(uint16_t us)
{
    hal_impl_delay_us(us);
}

/* ------------------------------------------------------------------ */
/*  Serial (UART on AVR, USB CDC on STM32)                             */
/* ------------------------------------------------------------------ */

void     hal_serial_init(uint32_t baud);
void     hal_serial_putc(char c);
char     hal_serial_getc(void);
int      hal_serial_has_data(void);
void     hal_serial_flush(void);

/* Called from arch-specific RX ISR (AVR only; no-op on STM32) */
void     hal_serial_rx_isr_handler(uint8_t byte);

/* ------------------------------------------------------------------ */
/*  GPIO                                                               */
/* ------------------------------------------------------------------ */

static inline void hal_gpio_set_output(uint8_t port, uint8_t pin)
{
    hal_impl_gpio_set_output(port, pin);
}

static inline void hal_gpio_set_input_pullup(uint8_t port, uint8_t pin)
{
    hal_impl_gpio_set_input_pullup(port, pin);
}

static inline void hal_gpio_set_high(uint8_t port, uint8_t pin)
{
    hal_impl_gpio_set_high(port, pin);
}

static inline void hal_gpio_set_low(uint8_t port, uint8_t pin)
{
    hal_impl_gpio_set_low(port, pin);
}

static inline uint8_t hal_gpio_read(uint8_t port, uint8_t pin)
{
    return hal_impl_gpio_read(port, pin);
}

/* ------------------------------------------------------------------ */
/*  Timer (free-running microsecond counter for BDM timeout)           */
/* ------------------------------------------------------------------ */

void     hal_timer_init(void);
uint32_t hal_timer_get_us(void);

#endif
