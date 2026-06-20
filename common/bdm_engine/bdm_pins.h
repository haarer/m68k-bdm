#ifndef BDM_PINS_H
#define BDM_PINS_H

#define STM32F411xE
#include "stm32f4xx.h"

/* ------------------------------------------------------------------ */
/*  Pin Mapping — STM32F411 Blackpill to CPU32 BDM bus                 */
/*                                                                     */
/*  DSCLK (BKPT)   PA0  (output)   — BDM serial clock                  */
/*  DSI  (IFETCH)  PA1  (output)   — data to target                    */
/*  TARGET_RESET   PA2  (output)   — target reset line (active low)   */
/*  DSO  (IPIPE)   PB0  (input)    — data from target                  */
/*  FREEZE         PB1  (input)    — BDM mode indicator (active low)  */
/* ------------------------------------------------------------------ */

#define BDM_DSCLK_GPIO      GPIOA
#define BDM_DSCLK_PIN       0

#define BDM_DSI_GPIO        GPIOA
#define BDM_DSI_PIN         1

#define BDM_RESET_GPIO      GPIOA
#define BDM_RESET_PIN       2

#define BDM_DSO_GPIO        GPIOB
#define BDM_DSO_PIN         0

#define BDM_FREEZE_GPIO     GPIOB
#define BDM_FREEZE_PIN      1

/* ------------------------------------------------------------------ */
/*  GPIO helpers — direct register access, no HAL                       */
/* ------------------------------------------------------------------ */

#define bdm_gpio_set(port, pin)     ((port)->BSRR = (uint32_t)(1 << (pin)))
#define bdm_gpio_clr(port, pin)     ((port)->BSRR = (uint32_t)(1 << ((pin) + 16)))
#define bdm_gpio_read(port, pin)    (((port)->IDR >> (pin)) & 1U)

#define bdm_gpio_set_output(port, pin) \
    ((port)->MODER = ((port)->MODER & ~(3UL << ((pin) * 2))) | (1UL << ((pin) * 2)))

#define bdm_gpio_set_input_pullup(port, pin) do { \
    (port)->MODER &= ~(3UL << ((pin) * 2)); \
    (port)->PUPDR = ((port)->PUPDR & ~(3UL << ((pin) * 2))) | (1UL << ((pin) * 2)); \
} while(0)

#endif
