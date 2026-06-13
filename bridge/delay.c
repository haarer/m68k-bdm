#include "delay.h"
#define STM32F411xE
#include "stm32f4xx.h"

extern uint32_t SystemCoreClock;

void delay_us(uint32_t us) {
    uint32_t total = SystemCoreClock / 1000000 * us;

    SysTick->CTRL = 0;

    while (total) {
        uint32_t reload = (total > SysTick_LOAD_RELOAD_Msk)
                        ? SysTick_LOAD_RELOAD_Msk : total;
        SysTick->LOAD = reload;
        SysTick->VAL  = 0;
        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
        SysTick->CTRL = 0;
        total -= reload;
    }
}

void delay_ms(uint32_t ms) {
    delay_us(ms * 1000);
}
