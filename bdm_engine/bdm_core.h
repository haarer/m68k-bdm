#ifndef BDM_CORE_H
#define BDM_CORE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BDM_STATE_IDLE,
    BDM_STATE_PREAMBLE,
    BDM_STATE_COMMAND,
    BDM_STATE_ADDRESS,
    BDM_STATE_DATA,
    BDM_STATE_CHECKSUM
} bdm_state_t;

typedef enum {
    BDM_SIZE_BYTE = 0,
    BDM_SIZE_WORD = 1,
    BDM_SIZE_LONG = 2
} bdm_size_t;

typedef enum {
    BDM_REG_D0  = 0, BDM_REG_D1  = 1, BDM_REG_D2  = 2, BDM_REG_D3  = 3,
    BDM_REG_D4  = 4, BDM_REG_D5  = 5, BDM_REG_D6  = 6, BDM_REG_D7  = 7,
    BDM_REG_A0  = 0, BDM_REG_A1  = 1, BDM_REG_A2  = 2, BDM_REG_A3  = 3,
    BDM_REG_A4  = 4, BDM_REG_A5  = 5, BDM_REG_A6  = 6, BDM_REG_A7  = 7,
} bdm_gpreg_t;

typedef enum {
    BDM_SYSREG_RPC   = 0,
    BDM_SYSREG_PCC   = 1,
    BDM_SYSREG_SR    = 3,
    BDM_SYSREG_USP   = 6,
    BDM_SYSREG_SSP   = 7,
    BDM_SYSREG_SFC   = 8,
    BDM_SYSREG_DFC   = 9,
    BDM_SYSREG_ATEMP = 2,
    BDM_SYSREG_FAR   = 3,
    BDM_SYSREG_VBR   = 4,
} bdm_sysreg_t;

typedef enum {
    BDM_OK,
    BDM_ERR_TIMEOUT,
    BDM_ERR_BERR,
    BDM_ERR_NOT_READY,
    BDM_ERR_ILLEGAL,
    BDM_ERR_NO_TARGET
} bdm_result_t;

/* ------------------------------------------------------------------ */
/*  Initialization                                                     */
/* ------------------------------------------------------------------ */

void     bdm_init(void);

/* Enable BDM on target: assert BKPT low, toggle RESET so BKPT samples
   low at the rising edge of RESET (CPU32 §7.2.1).  One-time call.     */
bool     bdm_enable(void);

/* ------------------------------------------------------------------ */
/*  Low-level serial protocol (CPU32 §7.2.7)                           */
/*      17-bit full-duplex words: 16 data bits + 1 status bit (DSO)   */
/*      DSCLK on BKPT, DSI on IFETCH, DSO on IPIPE                   */
/* ------------------------------------------------------------------ */

/* Shift a 16-bit word. poll=true waits for CPU ready (DSO bit 16 = 0).
   Returns the 16-bit data read from DSO. */
uint16_t bdm_shift_word(uint16_t out, bool poll);

/* Poll until CPU indicates ready (DSO bit 16 = 0), discarding data words. */
bool     bdm_poll_ready(void);

/* ------------------------------------------------------------------ */
/*  Memory access                                                      */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_memory(uint32_t addr, uint8_t size, uint32_t *data);
bdm_result_t bdm_write_memory(uint32_t addr, uint8_t size, uint32_t data);
bdm_result_t bdm_dump_memory(uint32_t addr, uint8_t size, uint8_t count, uint32_t *data);
bdm_result_t bdm_fill_memory(uint32_t addr, uint8_t size, uint32_t data, uint8_t count);

/* ------------------------------------------------------------------ */
/*  Register access                                                    */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_data_reg(uint8_t reg, uint32_t *value);
bdm_result_t bdm_write_data_reg(uint8_t reg, uint32_t value);
bdm_result_t bdm_read_addr_reg(uint8_t reg, uint32_t *value);
bdm_result_t bdm_write_addr_reg(uint8_t reg, uint32_t value);
bdm_result_t bdm_read_sysreg(uint8_t select, uint32_t *value);
bdm_result_t bdm_write_sysreg(uint8_t select, uint32_t value);

/* ------------------------------------------------------------------ */
/*  Control commands                                                   */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_target_reset(void);
bdm_result_t bdm_target_halt(void);
bdm_result_t bdm_target_go(void);
bdm_result_t bdm_call(uint32_t addr);
bdm_result_t bdm_step(void);
bdm_result_t bdm_nop(void);

/* ------------------------------------------------------------------ */
/*  State query                                                        */
/* ------------------------------------------------------------------ */

bool     bdm_in_bdm_mode(void);

#endif
