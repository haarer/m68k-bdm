#ifndef SIM_CORE_H
#define SIM_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/*  CPU32 BDM Target Simulator Core                                    */
/*                                                                     */
/*  Implements the CPU32 BDM slave: opcode decoding, register file,   */
/*  memory access, and status generation.                              */
/* ------------------------------------------------------------------ */

/* Initialize the target simulator */
void sim_core_init(void);

/* Run one BDM command cycle. Returns true if BDM mode is still active. */
bool sim_core_run(void);

/* Check if target is in BDM mode */
bool sim_core_in_bdm(void);

/* Get the current BDM entry source (ATEMP value) */
uint32_t sim_core_entry_source(void);

#endif
