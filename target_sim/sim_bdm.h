#ifndef SIM_BDM_H
#define SIM_BDM_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  Low-level BDM word transfer (target/slave side)                    */
/*                                                                     */
/*  The target samples DSI on DSCLK rising edge and drives DSO on     */
/*  DSCLK rising edge. 17 bits per word: 16 data + 1 status (bit 16). */
/* ------------------------------------------------------------------ */

/* Initialize BDM pins (DSCLK input, DSI input, DSO output, FREEZE output) */
void sim_bdm_init(void);

/* Shift one 17-bit word.
 * out_data: 16 bits to drive on DSO (ignored if out_ready is false)
 * out_ready: if true, bit 16 = 0 (READY); if false, bit 16 = 1 (NOT READY)
 * Returns: 16-bit data sampled from DSI
 */
uint16_t sim_bdm_shift_word(uint16_t out_data, bool out_ready);

/* Wait for preamble: DSCLK goes low (BKPT asserted by debugger) */
bool sim_bdm_wait_preamble(void);

/* Assert FREEZE (enter BDM mode) */
void sim_bdm_assert_freeze(void);

/* Deassert FREEZE (exit BDM mode) */
void sim_bdm_deassert_freeze(void);

/* Check if RESET is asserted (active low) */
bool sim_bdm_reset_asserted(void);

/* Sample BKPT (DSCLK) state */
bool sim_bdm_bkpt_sampled(void);

#endif
