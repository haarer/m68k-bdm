    .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb

.global g_pfnVectors
.global Default_Handler

/* ------------------------------------------------------------------ */
/*  Vector table                                                       */
/* ------------------------------------------------------------------ */

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMP_STAMP_IRQHandler
    .word RTC_WKUP_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Stream0_IRQHandler
    .word DMA1_Stream1_IRQHandler
    .word DMA1_Stream2_IRQHandler
    .word DMA1_Stream3_IRQHandler
    .word DMA1_Stream4_IRQHandler
    .word DMA1_Stream5_IRQHandler
    .word DMA1_Stream6_IRQHandler
    .word ADC_IRQHandler
    .word 0
    .word 0
    .word 0
    .word 0
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_TIM9_IRQHandler
    .word TIM1_UP_TIM10_IRQHandler
    .word TIM1_TRG_COM_TIM11_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART6_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTC_Alarm_IRQHandler
    .word OTG_FS_WKUP_IRQHandler
    .word 0
    .word 0
    .word 0
    .word 0
    .word DMA1_Stream7_IRQHandler
    .word 0
    .word SDIO_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word 0
    .word 0
    .word 0
    .word 0
    .word DMA2_Stream0_IRQHandler
    .word DMA2_Stream1_IRQHandler
    .word DMA2_Stream2_IRQHandler
    .word DMA2_Stream3_IRQHandler
    .word DMA2_Stream4_IRQHandler
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word OTG_FS_IRQHandler
    .word DMA2_Stream5_IRQHandler
    .word DMA2_Stream6_IRQHandler
    .word DMA2_Stream7_IRQHandler
    .word USART6_IRQHandler
    .word I2C3_EV_IRQHandler
    .word I2C3_ER_IRQHandler
    .word FPU_IRQHandler
    .word 0
    .word 0
    .word SPI4_IRQHandler
    .word SPI5_IRQHandler

/* ------------------------------------------------------------------ */
/*  Default handler                                                    */
/* ------------------------------------------------------------------ */

.weak NMI_Handler
.thumb_set NMI_Handler,Default_Handler

.weak HardFault_Handler
.thumb_set HardFault_Handler,Default_Handler

.weak MemManage_Handler
.thumb_set MemManage_Handler,Default_Handler

.weak BusFault_Handler
.thumb_set BusFault_Handler,Default_Handler

.weak UsageFault_Handler
.thumb_set UsageFault_Handler,Default_Handler

.weak SVC_Handler
.thumb_set SVC_Handler,Default_Handler

.weak DebugMon_Handler
.thumb_set DebugMon_Handler,Default_Handler

.weak PendSV_Handler
.thumb_set PendSV_Handler,Default_Handler

.weak WWDG_IRQHandler
.thumb_set WWDG_IRQHandler,Default_Handler

.weak PVD_IRQHandler
.thumb_set PVD_IRQHandler,Default_Handler

.weak TAMP_STAMP_IRQHandler
.thumb_set TAMP_STAMP_IRQHandler,Default_Handler

.weak RTC_WKUP_IRQHandler
.thumb_set RTC_WKUP_IRQHandler,Default_Handler

.weak FLASH_IRQHandler
.thumb_set FLASH_IRQHandler,Default_Handler

.weak RCC_IRQHandler
.thumb_set RCC_IRQHandler,Default_Handler

.weak EXTI0_IRQHandler
.thumb_set EXTI0_IRQHandler,Default_Handler

.weak EXTI1_IRQHandler
.thumb_set EXTI1_IRQHandler,Default_Handler

.weak EXTI2_IRQHandler
.thumb_set EXTI2_IRQHandler,Default_Handler

.weak EXTI3_IRQHandler
.thumb_set EXTI3_IRQHandler,Default_Handler

.weak EXTI4_IRQHandler
.thumb_set EXTI4_IRQHandler,Default_Handler

.weak DMA1_Stream0_IRQHandler
.thumb_set DMA1_Stream0_IRQHandler,Default_Handler

.weak DMA1_Stream1_IRQHandler
.thumb_set DMA1_Stream1_IRQHandler,Default_Handler

.weak DMA1_Stream2_IRQHandler
.thumb_set DMA1_Stream2_IRQHandler,Default_Handler

.weak DMA1_Stream3_IRQHandler
.thumb_set DMA1_Stream3_IRQHandler,Default_Handler

.weak DMA1_Stream4_IRQHandler
.thumb_set DMA1_Stream4_IRQHandler,Default_Handler

.weak DMA1_Stream5_IRQHandler
.thumb_set DMA1_Stream5_IRQHandler,Default_Handler

.weak DMA1_Stream6_IRQHandler
.thumb_set DMA1_Stream6_IRQHandler,Default_Handler

.weak ADC_IRQHandler
.thumb_set ADC_IRQHandler,Default_Handler

.weak EXTI9_5_IRQHandler
.thumb_set EXTI9_5_IRQHandler,Default_Handler

.weak TIM1_BRK_TIM9_IRQHandler
.thumb_set TIM1_BRK_TIM9_IRQHandler,Default_Handler

.weak TIM1_UP_TIM10_IRQHandler
.thumb_set TIM1_UP_TIM10_IRQHandler,Default_Handler

.weak TIM1_TRG_COM_TIM11_IRQHandler
.thumb_set TIM1_TRG_COM_TIM11_IRQHandler,Default_Handler

.weak TIM1_CC_IRQHandler
.thumb_set TIM1_CC_IRQHandler,Default_Handler

.weak TIM2_IRQHandler
.thumb_set TIM2_IRQHandler,Default_Handler

.weak TIM3_IRQHandler
.thumb_set TIM3_IRQHandler,Default_Handler

.weak TIM4_IRQHandler
.thumb_set TIM4_IRQHandler,Default_Handler

.weak I2C1_EV_IRQHandler
.thumb_set I2C1_EV_IRQHandler,Default_Handler

.weak I2C1_ER_IRQHandler
.thumb_set I2C1_ER_IRQHandler,Default_Handler

.weak I2C2_EV_IRQHandler
.thumb_set I2C2_EV_IRQHandler,Default_Handler

.weak I2C2_ER_IRQHandler
.thumb_set I2C2_ER_IRQHandler,Default_Handler

.weak SPI1_IRQHandler
.thumb_set SPI1_IRQHandler,Default_Handler

.weak SPI2_IRQHandler
.thumb_set SPI2_IRQHandler,Default_Handler

.weak USART1_IRQHandler
.thumb_set USART1_IRQHandler,Default_Handler

.weak USART2_IRQHandler
.thumb_set USART2_IRQHandler,Default_Handler

.weak USART6_IRQHandler
.thumb_set USART6_IRQHandler,Default_Handler

.weak EXTI15_10_IRQHandler
.thumb_set EXTI15_10_IRQHandler,Default_Handler

.weak RTC_Alarm_IRQHandler
.thumb_set RTC_Alarm_IRQHandler,Default_Handler

.weak OTG_FS_WKUP_IRQHandler
.thumb_set OTG_FS_WKUP_IRQHandler,Default_Handler

.weak DMA1_Stream7_IRQHandler
.thumb_set DMA1_Stream7_IRQHandler,Default_Handler

.weak SDIO_IRQHandler
.thumb_set SDIO_IRQHandler,Default_Handler

.weak TIM5_IRQHandler
.thumb_set TIM5_IRQHandler,Default_Handler

.weak SPI3_IRQHandler
.thumb_set SPI3_IRQHandler,Default_Handler

.weak DMA2_Stream0_IRQHandler
.thumb_set DMA2_Stream0_IRQHandler,Default_Handler

.weak DMA2_Stream1_IRQHandler
.thumb_set DMA2_Stream1_IRQHandler,Default_Handler

.weak DMA2_Stream2_IRQHandler
.thumb_set DMA2_Stream2_IRQHandler,Default_Handler

.weak DMA2_Stream3_IRQHandler
.thumb_set DMA2_Stream3_IRQHandler,Default_Handler

.weak DMA2_Stream4_IRQHandler
.thumb_set DMA2_Stream4_IRQHandler,Default_Handler

.weak OTG_FS_IRQHandler
.thumb_set OTG_FS_IRQHandler,Default_Handler

.weak DMA2_Stream5_IRQHandler
.thumb_set DMA2_Stream5_IRQHandler,Default_Handler

.weak DMA2_Stream6_IRQHandler
.thumb_set DMA2_Stream6_IRQHandler,Default_Handler

.weak DMA2_Stream7_IRQHandler
.thumb_set DMA2_Stream7_IRQHandler,Default_Handler

.weak I2C3_EV_IRQHandler
.thumb_set I2C3_EV_IRQHandler,Default_Handler

.weak I2C3_ER_IRQHandler
.thumb_set I2C3_ER_IRQHandler,Default_Handler

.weak FPU_IRQHandler
.thumb_set FPU_IRQHandler,Default_Handler

.weak SPI4_IRQHandler
.thumb_set SPI4_IRQHandler,Default_Handler

.weak SPI5_IRQHandler
.thumb_set SPI5_IRQHandler,Default_Handler

Default_Handler:
    b .

/* ------------------------------------------------------------------ */
/*  Reset handler — clock setup, BSS/Data init, jump to main           */
/* ------------------------------------------------------------------ */

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:

    /* Set stack pointer */
    ldr r0, =_estack
    mov sp, r0

    /* Enable HSE external crystal (25MHz) — required for accurate USB clock */
    ldr r0, =0x40023800    /* RCC base */
    ldr r1, [r0]
    orr r1, r1, #(1 << 16) /* HSEON */
    str r1, [r0]
1:
    ldr r1, [r0]
    tst r1, #(1 << 17)     /* HSERDY */
    beq 1b

    /* Configure PLLCFGR: HSE=25MHz, PLL_M=25, PLL_N=384, PLL_P=4, PLL_Q=8, SRC=1 (HSE)
     * VCO = 25/25*384 = 384MHz
     * SYSCLK = 384/4 = 96MHz
     * USB = 384/8 = 48MHz (exact — HSE crystal provides required accuracy) */
    ldr r1, =(25 << 0) | (384 << 6) | (0x1 << 16) | (8 << 24) | (1 << 22)
    str r1, [r0, #4]       /* RCC_PLLCFGR */

    /* Enable PLL */
    ldr r1, [r0]
    orr r1, r1, #(1 << 24) /* PLLON */
    str r1, [r0]
1:
    ldr r1, [r0]
    tst r1, #(1 << 25)     /* PLLRDY */
    beq 1b

    /* Set flash latency to 3 wait states (for 96MHz) */
    ldr r0, =0x40023C00    /* FLASH base */
    ldr r1, [r0]
    orr r1, r1, #(0x3 << 0) /* LATENCY=3 */
    orr r1, r1, #(1 << 8)   /* PRFTEN */
    orr r1, r1, #(1 << 9)   /* ICEN */
    orr r1, r1, #(1 << 10)  /* DCEN */
    str r1, [r0]

    /* Configure APB prescalers before switching clock */
    ldr r0, =0x40023800    /* RCC base */
    ldr r1, [r0, #8]       /* RCC_CFGR */
    orr r1, r1, #(0x5 << 10) /* APB1 = DIV4 (24MHz) */
    orr r1, r1, #(0x4 << 13) /* APB2 = DIV2 (48MHz) */
    str r1, [r0, #8]

    /* Switch to PLL as system clock */
    ldr r1, [r0, #8]
    orr r1, r1, #(0x2 << 0) /* SW = PLL */
    str r1, [r0, #8]
2:
    ldr r1, [r0, #8]
    tst r1, #(0x2 << 2)    /* SWS = PLL */
    beq 2b

    /* Initialize .data */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
4:
    cmp r1, r2
    itt lt
    ldrlt r3, [r0], #4
    strlt r3, [r1], #4
    blt 4b

    /* Zero .bss */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
5:
    cmp r0, r1
    itt lt
    strlt r2, [r0], #4
    blt 5b

    /* Jump to main */
    bl main
    b .

.size Reset_Handler, .-Reset_Handler
