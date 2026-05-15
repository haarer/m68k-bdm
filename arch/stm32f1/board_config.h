#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Board                                                              */
/* ------------------------------------------------------------------ */

#define BOARD_BLACKPILL
#define F_CPU 72000000UL

/* ------------------------------------------------------------------ */
/*  Serial (Host side — USB CDC, baud is ignored)                      */
/* ------------------------------------------------------------------ */

#define SERIAL_BAUD 115200UL

/* ------------------------------------------------------------------ */
/*  BDM Pin port/pin definitions (STM32F103C8T6)                       */
/*      Per CPU32 Reference Manual §7.2.7:                             */
/*        BKPT  -> DSCLK  (serial clock, output from bridge)           */
/*        IFETCH-> DSI    (serial data in to CPU, output from bridge)  */
/*        IPIPE -> DSO    (serial data out from CPU, input to bridge)  */
/*        FREEZE indicates CPU has entered BDM (input to bridge)       */
/* ------------------------------------------------------------------ */

/* Port encoding: 0=GPIOA, 1=GPIOB, 2=GPIOC */
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

/* BDREQ / BDMACK handshake pins (configured but not actively used) */
#define BDREQ_PORT       GPIO_A
#define BDREQ_PIN        5

#define BDMACK_PORT      GPIO_A
#define BDMACK_PIN       6

#endif
