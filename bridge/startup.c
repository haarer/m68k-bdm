#define STM32F411xE
#include "stm32f4xx.h"

extern int main(void);

extern char _sidata, _sdata, _edata, _sbss, _ebss, _estack;

uint32_t SystemCoreClock = 16000000;

void Default_Handler(void) {
    for (;;) __WFE();
}

void Reset_Handler(void);
void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)__attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void USART1_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* ---------------------------------------------------------------------------
 * SystemInit  —  clock tree initialisation (CMSIS convention)
 *
 * Target: 100 MHz from either HSE (25 MHz) or HSI (16 MHz) via PLL.
 *
 * Clock tree (RM0383 §6.2, §6.3):
 *
 *   HSE (25 MHz) ─┬─> PLLSRC ─> PLLM(/25) ─> VCO_in (1 MHz)
 *                  │               └─ or PLLM(/16) from HSI
 *                  └─> RTC etc.
 *
 *   VCO_in (1 MHz) ─> PLLN(x200) ─> VCO_out (200 MHz)
 *                                      │
 *                                      ├─> PLLP(/2) ─> SYSCLK (100 MHz)
 *                                      ├─> PLLQ(/7) ─> 48 MHz (USB SDIO)
 *                                      └─> PLLR         (48xxx only)
 *
 *   SYSCLK (100 MHz) ─> AHB presc(/1) ─> HCLK  (100 MHz)
 *                       ├─> APB1 presc(/2) ─> PCLK1 (50 MHz)   (§6.4.1)
 *                       └─> APB2 presc(/1) ─> PCLK2 (100 MHz)  (§6.4.1)
 *
 * Flash: 3 wait states required at 100 MHz in Scale 1 mode (§3.5.1 Table 10)
 * Power: VOS = Scale 1 for operation up to 100 MHz (§3.3.1 Table 6)
 * --------------------------------------------------------------------------- */

#define HSE_TIMEOUT 50000

void SystemInit(void) {
    int use_hse = 0;

    /* Enable HSE oscillator with timeout (§6.2.2) */
    RCC->CR |= RCC_CR_HSEON;
    for (int i = 0; i < HSE_TIMEOUT; i++) {
        if (RCC->CR & RCC_CR_HSERDY) { use_hse = 1; break; }
    }

    if (!use_hse) {
        RCC->CR &= ~RCC_CR_HSEON;
    }

    /* Enable power interface clock and set VOS to Scale 1 (§3.3.1) */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR = (PWR->CR & ~PWR_CR_VOS) | (PWR_CR_VOS_0 | PWR_CR_VOS_1);

    /* Configure flash: 3 wait states, prefetch + icache + dcache (§3.5.1) */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN
               | FLASH_ACR_LATENCY_3WS;

    /* AHB = SYSCLK / 1, APB1 = HCLK / 2, APB2 = HCLK / 1 (§6.4.1) */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;

    /* PLL from HSE (M=25) or HSI (M=16), N=200, P=/2, Q=/7 (§6.3) */
    if (use_hse) {
        RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE
                     | (25 << RCC_PLLCFGR_PLLM_Pos)
                     | (200 << RCC_PLLCFGR_PLLN_Pos)
                     | (0 << RCC_PLLCFGR_PLLP_Pos)
                     | (7  << RCC_PLLCFGR_PLLQ_Pos);
    } else {
        RCC->PLLCFGR = (0 << RCC_PLLCFGR_PLLSRC_Pos)
                     | (16 << RCC_PLLCFGR_PLLM_Pos)
                     | (200 << RCC_PLLCFGR_PLLN_Pos)
                     | (0 << RCC_PLLCFGR_PLLP_Pos)
                     | (7  << RCC_PLLCFGR_PLLQ_Pos);
    }

    /* Enable PLL and wait for lock (§6.3.2) */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* Select PLL as system clock and wait for switch (§6.4.1) */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    SystemCoreClock = 100000000;
}

__attribute__((section(".isr_vector"))) void (*const vector_table[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    [16 + 37] = USART1_Handler,
};

void Reset_Handler(void) {
    volatile char *src = &_sidata;
    volatile char *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;

    SCB->VTOR = (uintptr_t)vector_table;

    SystemInit();

    /* Enable FPU (CP10, CP11 full access) before any hard-float code runs */
    SCB->CPACR |= 0xF << 20;
    __DSB();
    __ISB();

    main();

    for (;;) __NOP();
}
