#define STM32F411xE
#include "stm32f4xx.h"
#include "config.h"
#include "bdm_timing.h"
#include "delay.h"

extern uint32_t SystemCoreClock;

static uint32_t timeout_start_cycle;

void bdm_timing_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    timeout_start_cycle = 0;
}

void bdm_delay_half_period(void)
{
    delay_us(BDM_CLOCK_HALF_US);
}

void bdm_delay_full_period(void)
{
    delay_us(BDM_CLOCK_HALF_US * 2);
}

void bdm_delay_us(uint16_t us)
{
    delay_us(us);
}

void bdm_timeout_start(void)
{
    timeout_start_cycle = DWT->CYCCNT;
}

bool bdm_timeout_exceeded(void)
{
    uint32_t elapsed = DWT->CYCCNT - timeout_start_cycle;
    uint32_t elapsed_us = elapsed / (SystemCoreClock / 1000000UL);
    return elapsed_us >= (BDM_TIMEOUT_MS * 1000UL);
}
