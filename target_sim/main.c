#include "hal.h"
#include "sim_core.h"
#include "sim_debug.h"

/* ------------------------------------------------------------------ */
/*  CPU32 BDM Target Simulator — Main Entry Point                      */
/*                                                                     */
/*  Runs on Blackpill #2, simulating an MC68331 CPU32 target.         */
/*  Communicates with bridge (Blackpill #1) via BDM signals.          */
/*  Debug output via USB CDC — non-blocking, drained between cmds.    */
/* ------------------------------------------------------------------ */

int main(void)
{
    hal_timer_init();
    hal_serial_init(0);  /* USB CDC (baud ignored) */
    dbg_init();

    sim_core_init();

    hal_irq_enable();

    while (1) {
        sim_core_run();
        dbg_drain();  /* Drain debug buffer between BDM commands — never blocks */
    }

    return 0;
}
