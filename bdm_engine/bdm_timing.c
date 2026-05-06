#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "config.h"
#include "bdm_timing.h"

static uint32_t timeout_start_ms;

void bdm_timing_init(void)
{
    timeout_start_ms = 0;
}

void bdm_delay_half_period(void)
{
    _delay_us(BDM_CLOCK_HALF_US);
}

void bdm_delay_full_period(void)
{
    _delay_us(BDM_CLOCK_HALF_US * 2);
}

void bdm_delay_us(uint16_t us)
{
    for (uint16_t i = 0; i < us; i++)
        _delay_us(1);
}

/* ------------------------------------------------------------------ */
/*  Timeout tracking using TCNT1 (free-running at F_CPU)              */
/* ------------------------------------------------------------------ */

void bdm_timeout_start(void)
{
    timeout_start_ms = (uint32_t)TCNT1;
}

bool bdm_timeout_exceeded(void)
{
    uint32_t now = (uint32_t)TCNT1;
    uint32_t elapsed_us = (now - timeout_start_ms) * (1000000UL / F_CPU);
    return (elapsed_us >= (BDM_TIMEOUT_MS * 1000UL));
}
