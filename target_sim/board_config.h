#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Board                                                              */
/* ------------------------------------------------------------------ */

#define BOARD_TARGET_SIM
#define F_CPU 72000000UL

/* ------------------------------------------------------------------ */
/*  BDM Pin port/pin definitions (STM32F103C8T6 — Target Simulator)   */
/*      Mirrors bridge pin mapping for direct wire connection          */
/* ------------------------------------------------------------------ */

#define GPIO_A 0
#define GPIO_B 1
#define GPIO_C 2

#define DSCLK_PORT       GPIO_A
#define DSCLK_PIN        0

#define DSI_PORT         GPIO_A
#define DSI_PIN          1

#define DSO_PORT         GPIO_A
#define DSO_PIN          2

#define FREEZE_PORT      GPIO_A
#define FREEZE_PIN       3

#define TARGET_RESET_PORT GPIO_A
#define TARGET_RESET_PIN  4

#endif
