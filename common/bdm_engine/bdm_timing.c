#include "config.h"
#include "hal.h"
#include "bdm_timing.h"

static uint32_t timeout_start_ticks;

void bdm_timing_init(void)
{
    timeout_start_ticks = 0;
}

void bdm_delay_half_period(void)
{
    hal_delay_us(BDM_CLOCK_HALF_US);
}

void bdm_delay_full_period(void)
{
    hal_delay_us(BDM_CLOCK_HALF_US * 2);
}

void bdm_delay_us(uint16_t us)
{
    hal_delay_us(us);
}

/* ------------------------------------------------------------------ */
/*  Timeout tracking using hal_timer (raw tick counter)                */
/*  Note: tick-to-us conversion uses integer division; on fast CPUs   */
/*  (F_CPU >> 1MHz) this may underflow. Primary timeout is the loop   */
/*  iteration count in the polling functions.                          */
/* ------------------------------------------------------------------ */

void bdm_timeout_start(void)
{
    timeout_start_ticks = hal_timer_get_us();
}

bool bdm_timeout_exceeded(void)
{
    uint32_t now = hal_timer_get_us();
    uint32_t elapsed_ticks = now - timeout_start_ticks;
    uint32_t elapsed_us = elapsed_ticks * (1000000UL / F_CPU);
    return (elapsed_us >= (BDM_TIMEOUT_MS * 1000UL));
}
