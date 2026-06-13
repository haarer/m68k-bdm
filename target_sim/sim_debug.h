#ifndef SIM_DEBUG_H
#define SIM_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  Non-blocking debug logger for target simulator                     */
/*                                                                     */
/*  Events are enqueued into a ring buffer during BDM operations      */
/*  (zero blocking). The main loop drains the buffer to UART          */
/*  between commands, so serial output never interferes with BDM      */
/*  timing.                                                           */
/* ------------------------------------------------------------------ */

#define DBG_BUF_SIZE 256

/* Initialize debug logger (call after hal_serial_init) */
void dbg_init(void);

/* Enqueue a debug event. Non-blocking — drops oldest if buffer full. */
void dbg_log(const char *fmt, ...);

/* Drain buffered events to USB CDC. Call from main loop between BDM commands. */
void dbg_drain(void);

/* Check if buffer has pending data */
bool dbg_pending(void);

/* Event types for structured logging */
typedef enum {
    DBG_BDM_ENTRY,
    DBG_BDM_EXIT,
    DBG_OPCODE,
    DBG_REG_READ,
    DBG_REG_WRITE,
    DBG_MEM_READ,
    DBG_MEM_WRITE,
    DBG_STATUS,
    DBG_ERROR,
    DBG_INFO
} dbg_event_t;

/* Structured event enqueue (faster than formatted string) */
void dbg_event(dbg_event_t type, uint16_t opcode, uint32_t arg1, uint32_t arg2);

#endif
