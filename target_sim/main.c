#include "hal.h"
#include "sim_core.h"

/* ------------------------------------------------------------------ */
/*  CPU32 BDM Target Simulator — Main Entry Point                      */
/*                                                                     */
/*  Runs on Blackpill #2, simulating an MC68331 CPU32 target.         */
/*  Communicates with bridge (Blackpill #1) via BDM signals.          */
/* ------------------------------------------------------------------ */

int main(void)
{
    hal_timer_init();
    sim_core_init();

    hal_irq_enable();

    while (1) {
        sim_core_run();
    }

    return 0;
}
