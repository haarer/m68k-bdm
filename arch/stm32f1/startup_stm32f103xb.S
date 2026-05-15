    .syntax unified
    .cpu cortex-m3
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
    .word TAMPER_IRQHandler
    .word RTC_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Channel1_IRQHandler
    .word DMA1_Channel2_IRQHandler
    .word DMA1_Channel3_IRQHandler
    .word DMA1_Channel4_IRQHandler
    .word DMA1_Channel5_IRQHandler
    .word DMA1_Channel6_IRQHandler
    .word DMA1_Channel7_IRQHandler
    .word ADC1_2_IRQHandler
    .word USB_HP_CAN1_TX_IRQHandler
    .word USB_LP_CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_IRQHandler
    .word TIM1_UP_IRQHandler
    .word TIM1_TRG_COM_IRQHandler
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
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTCAlarm_IRQHandler
    .word USBWakeUp_IRQHandler

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

.weak TAMPER_IRQHandler
.thumb_set TAMPER_IRQHandler,Default_Handler

.weak RTC_IRQHandler
.thumb_set RTC_IRQHandler,Default_Handler

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

.weak DMA1_Channel1_IRQHandler
.thumb_set DMA1_Channel1_IRQHandler,Default_Handler

.weak DMA1_Channel2_IRQHandler
.thumb_set DMA1_Channel2_IRQHandler,Default_Handler

.weak DMA1_Channel3_IRQHandler
.thumb_set DMA1_Channel3_IRQHandler,Default_Handler

.weak DMA1_Channel4_IRQHandler
.thumb_set DMA1_Channel4_IRQHandler,Default_Handler

.weak DMA1_Channel5_IRQHandler
.thumb_set DMA1_Channel5_IRQHandler,Default_Handler

.weak DMA1_Channel6_IRQHandler
.thumb_set DMA1_Channel6_IRQHandler,Default_Handler

.weak DMA1_Channel7_IRQHandler
.thumb_set DMA1_Channel7_IRQHandler,Default_Handler

.weak ADC1_2_IRQHandler
.thumb_set ADC1_2_IRQHandler,Default_Handler

.weak USB_HP_CAN1_TX_IRQHandler
.thumb_set USB_HP_CAN1_TX_IRQHandler,Default_Handler

.weak CAN1_RX1_IRQHandler
.thumb_set CAN1_RX1_IRQHandler,Default_Handler

.weak CAN1_SCE_IRQHandler
.thumb_set CAN1_SCE_IRQHandler,Default_Handler

.weak EXTI9_5_IRQHandler
.thumb_set EXTI9_5_IRQHandler,Default_Handler

.weak TIM1_BRK_IRQHandler
.thumb_set TIM1_BRK_IRQHandler,Default_Handler

.weak TIM1_UP_IRQHandler
.thumb_set TIM1_UP_IRQHandler,Default_Handler

.weak TIM1_TRG_COM_IRQHandler
.thumb_set TIM1_TRG_COM_IRQHandler,Default_Handler

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

.weak USART3_IRQHandler
.thumb_set USART3_IRQHandler,Default_Handler

.weak EXTI15_10_IRQHandler
.thumb_set EXTI15_10_IRQHandler,Default_Handler

.weak RTCAlarm_IRQHandler
.thumb_set RTCAlarm_IRQHandler,Default_Handler

.weak USBWakeUp_IRQHandler
.thumb_set USBWakeUp_IRQHandler,Default_Handler

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

    /* Enable HSE and wait for ready */
    ldr r0, =0x40021000    /* RCC base */
    ldr r1, [r0]           /* RCC_CR */
    orr r1, r1, #(1 << 16) /* HSEON */
    str r1, [r0]
1:
    ldr r1, [r0]
    tst r1, #(1 << 17)     /* HSERDY */
    beq 1b

    /* Configure PLL: HSE as source, x9 multiplier → 72 MHz */
    ldr r1, [r0, #4]       /* RCC_CFGR */
    ldr r2, =(0x7 << 18)   /* PLLMULL9 */
    orr r1, r1, r2
    orr r1, r1, #(1 << 16) /* PLLSRC (HSE) */
    orr r1, r1, #(0x4 << 8) /* PPRE1 = DIV2 (APB1 = 36 MHz) */
    str r1, [r0, #4]

    /* Enable PLL */
    ldr r1, [r0]
    orr r1, r1, #(1 << 24) /* PLLON */
    str r1, [r0]
2:
    ldr r1, [r0]
    tst r1, #(1 << 25)     /* PLLRDY */
    beq 2b

    /* Set flash latency to 2 wait states (for 72 MHz) */
    ldr r0, =0x40022000    /* FLASH base */
    ldr r1, [r0]
    orr r1, r1, #(0x2 << 0) /* LATENCY=2 */
    orr r1, r1, #(1 << 4)   /* PRFTBE */
    str r1, [r0]

    /* Switch to PLL as system clock */
    ldr r0, =0x40021000    /* RCC base */
    ldr r1, [r0, #4]
    orr r1, r1, #(0x2 << 0) /* SW = PLL */
    str r1, [r0, #4]
3:
    ldr r1, [r0, #4]
    tst r1, #(0x2 << 2)    /* SWS = PLL */
    beq 3b

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
