#ifndef CONFIG_H
#define CONFIG_H

#include <avr/io.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Board                                                              */
/* ------------------------------------------------------------------ */

#define BOARD_MEGA2560
#define F_CPU 16000000UL

/* ------------------------------------------------------------------ */
/*  Serial (Host side - USART0)                                        */
/* ------------------------------------------------------------------ */

#define SERIAL_BAUD       115200UL
#define SERIAL_UBRR_VALUE ((F_CPU / 16 / SERIAL_BAUD - 1) / 1)

/* ------------------------------------------------------------------ */
/*  BDM Pins (Target side - ATmega2560 port mapping)                   */
/* ------------------------------------------------------------------ */

#define BDMC_PORT     PORTA
#define BDMC_DDR      DDRA
#define BDMC_PIN      PINA
#define BDMC_BIT      0

#define BDD_PORT      PORTA
#define BDD_DDR       DDRA
#define BDD_PIN       PINA
#define BDD_BIT       1

#define BDREQ_PORT    PORTB
#define BDREQ_DDR     DDRB
#define BDREQ_PIN     PINB
#define BDREQ_BIT     0

#define BDMACK_PORT   PORTB
#define BDMACK_DDR    DDRB
#define BDMACK_PIN    PINB
#define BDMACK_BIT    1

#define TARGET_RESET_PORT  PORTA
#define TARGET_RESET_DDR   DDRA
#define TARGET_RESET_BIT   2

/* ------------------------------------------------------------------ */
/*  BDM Timing                                                         */
/* ------------------------------------------------------------------ */

#define BDM_CLOCK_KHZ         500
#define BDM_CLOCK_HALF_US     (1000 / BDM_CLOCK_KHZ)

/* ------------------------------------------------------------------ */
/*  Protocol                                                           */
/* ------------------------------------------------------------------ */

#define PROTOCOL_STX    0x02
#define PROTOCOL_ETX    0x03
#define PROTOCOL_MAX_PAYLOAD  256

/* ------------------------------------------------------------------ */
/*  Commands                                                           */
/* ------------------------------------------------------------------ */

#define CMD_MEM_READ       0x01
#define CMD_MEM_WRITE      0x02
#define CMD_REG_READ       0x03
#define CMD_REG_WRITE      0x04
#define CMD_TARGET_RESET   0x05
#define CMD_TARGET_HALT    0x06
#define CMD_TARGET_GO      0x07
#define CMD_STEP           0x08
#define CMD_BREAKPOINT_SET 0x09
#define CMD_BREAKPOINT_CLR 0x0A
#define CMD_STATUS         0x0B
#define CMD_CONFIG         0x0C

/* ------------------------------------------------------------------ */
/*  Responses                                                          */
/* ------------------------------------------------------------------ */

#define RSP_OK            0x00
#define RSP_ERROR         0x01
#define RSP_NOT_SUPPORTED 0x02
#define RSP_TIMEOUT       0x03
#define RSP_TARGET_ERROR  0x04

/* ------------------------------------------------------------------ */
/*  Buffers                                                            */
/* ------------------------------------------------------------------ */

#define RX_BUFFER_SIZE 256
#define TX_BUFFER_SIZE 256

#endif
