#include "board_config.h"
#include "delay.h"
#define STM32F411xE
#include "stm32f4xx.h"
#include "sim_bdm.h"
#include "sim_debug.h"

static uint32_t word_count = 0;

void sim_bdm_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;

    GPIO_TypeDef *pa = GPIOA;
    GPIO_TypeDef *pb = GPIOB;

    pa->MODER &= ~(3U << (0 * 2));
    pa->PUPDR &= ~(3U << (0 * 2));
    pa->PUPDR |=  (1U << (0 * 2));

    pa->MODER &= ~(3U << (1 * 2));
    pa->PUPDR &= ~(3U << (1 * 2));
    pa->PUPDR |=  (1U << (1 * 2));

    pa->MODER &= ~(3U << (2 * 2));
    pa->PUPDR &= ~(3U << (2 * 2));
    pa->PUPDR |=  (1U << (2 * 2));

    pb->MODER &= ~(3U << (0 * 2));
    pb->MODER |=  (1U << (0 * 2));
    pb->BSRR   =  (1U << 0);

    pb->MODER &= ~(3U << (1 * 2));
    pb->MODER |=  (1U << (1 * 2));
    pb->BSRR   =  (1U << 1);
}

uint16_t sim_bdm_shift_word(uint16_t out_data, bool out_ready)
{
    uint16_t data_in = 0;
    GPIO_TypeDef *pa = GPIOA;
    GPIO_TypeDef *pb = GPIOB;

    for (uint8_t bit = 0; bit < 17; bit++) {
        uint8_t dso_bit;
        if (bit < 16)
            dso_bit = (out_data >> (15 - bit)) & 1;
        else
            dso_bit = out_ready ? 0 : 1;

        if (dso_bit)
            pb->BSRR = (1U << 0);
        else
            pb->BSRR = (1U << 0) << 16;

        while (pa->IDR & (1U << 0))
            ;

        while (!(pa->IDR & (1U << 0)))
            ;

        if (bit < 16) {
            if (pa->IDR & (1U << 1))
                data_in |= (uint16_t)(1 << (15 - bit));
        }
    }

    pb->BSRR = (1U << 0);

    dbg_log("[word] #%u in=0x%04X out=0x%04X st=%u\n", (uint32_t)word_count++, data_in, out_data, out_ready ? 0U : 1U);
    return data_in;
}

bool sim_bdm_wait_preamble(void)
{
    uint32_t timeout = 100000;
    while (GPIOA->IDR & (1U << 0)) {
        if (--timeout == 0) {
            dbg_log("[preamble] timeout\n");
            return false;
        }
        delay_us(10);
    }
    dbg_log("[preamble] detected\n");
    return true;
}

void sim_bdm_assert_freeze(void)
{
    GPIOB->BSRR = (1U << 1) << 16;
    dbg_log("[freeze] asserted\n");
}

void sim_bdm_deassert_freeze(void)
{
    GPIOB->BSRR = (1U << 1);
    dbg_log("[freeze] deasserted\n");
}

bool sim_bdm_reset_asserted(void)
{
    return (GPIOA->IDR & (1U << 2)) == 0;
}

bool sim_bdm_bkpt_sampled(void)
{
    return (GPIOA->IDR & (1U << 0)) == 0;
}
