#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <avr/io.h>

/* ------------------------------------------------------------------ */
/*  Board                                                              */
/* ------------------------------------------------------------------ */

#define BOARD_MEGA2560
#define F_CPU 16000000UL

/* ------------------------------------------------------------------ */
/*  Serial (Host side - USART0)                                        */
/* ------------------------------------------------------------------ */

#define SERIAL_BAUD       115200UL
#define SERIAL_UBRR_VALUE (F_CPU / (8 * SERIAL_BAUD) - 1)

/* ------------------------------------------------------------------ */
/*  BDM Pin port/pid definitions (ATmega2560)                          */
/*      Per CPU32 Reference Manual §7.2.7:                             */
/*        BKPT  -> DSCLK  (serial clock, output from bridge)           */
/*        IFETCH-> DSI    (serial data in to CPU, output from bridge)  */
/*        IPIPE -> DSO    (serial data out from CPU, input to bridge)  */
/*        FREEZE indicates CPU has entered BDM (input to bridge)       */
/* ------------------------------------------------------------------ */

#define DSCLK_PORT       5   /* PORTF */
#define DSCLK_PIN        1

#define DSI_PORT         5   /* PORTF */
#define DSI_PIN          0

#define DSO_PORT         5   /* PORTF */
#define DSO_PIN          4

#define FREEZE_PORT      5   /* PORTF */
#define FREEZE_PIN       3

#define TARGET_RESET_PORT 5  /* PORTF */
#define TARGET_RESET_PIN 2

#endif
