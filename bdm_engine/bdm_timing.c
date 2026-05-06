#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "config.h"
#include "bdm_timing.h"

void bdm_timing_init(void)
{
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
