#ifndef HAL_IMPL_H
#define HAL_IMPL_H

#include <stdint.h>
#include "board_config.h"

/* ------------------------------------------------------------------ */
/*  STM32F411CEU6 register definitions (no external CMSIS needed)      */
/* ------------------------------------------------------------------ */

#define PERIPH_BASE     0x40000000UL
#define AHB1_BASE       (PERIPH_BASE + 0x20000UL)
#define AHB2_BASE       (PERIPH_BASE + 0x40000UL)
#define APB1_BASE       (PERIPH_BASE)
#define APB2_BASE       (PERIPH_BASE + 0x10000UL)

/* GPIO */
#define GPIOA_BASE      (AHB1_BASE + 0x0000UL)
#define GPIOB_BASE      (AHB1_BASE + 0x0400UL)
#define GPIOC_BASE      (AHB1_BASE + 0x0800UL)

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

#define GPIOA  ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB  ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC  ((GPIO_TypeDef *)GPIOC_BASE)

/* RCC */
#define RCC_BASE        (AHB1_BASE + 0x3800UL)
typedef struct {
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t RESERVED0[2];
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t RESERVED2[2];
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t RESERVED3[2];
    volatile uint32_t AHB1LPENR;
    volatile uint32_t AHB2LPENR;
    volatile uint32_t RESERVED4[2];
    volatile uint32_t APB1LPENR;
    volatile uint32_t APB2LPENR;
    volatile uint32_t RESERVED5[2];
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
    volatile uint32_t RESERVED6[2];
    volatile uint32_t SSCGR;
    volatile uint32_t PLLI2SCFGR;
} RCC_TypeDef;

#define RCC  ((RCC_TypeDef *)RCC_BASE)

#define RCC_CR_PLLRDY        (1 << 25)
#define RCC_CR_PLLON         (1 << 24)
#define RCC_CR_HSERDY        (1 << 17)
#define RCC_CR_HSEON         (1 << 16)
#define RCC_CR_HSIRDY        (1 << 1)
#define RCC_CR_HSION         (1 << 0)

#define RCC_PLLCFGR_PLLQ_7   (0x7 << 24)
#define RCC_PLLCFGR_PLLQ_SHIFT 24
#define RCC_PLLCFGR_PLLSRC   (1 << 22)
#define RCC_PLLCFGR_PLLP_4   (0x1 << 16)  /* 0b01 = /4 */
#define RCC_PLLCFGR_PLLN_400 (400 << 6)
#define RCC_PLLCFGR_PLLM_25  (25 << 0)

#define RCC_CFGR_PPRE1_DIV4  (0x5 << 10)  /* APB1 = 25MHz */
#define RCC_CFGR_PPRE2_DIV2  (0x4 << 13)  /* APB2 = 50MHz */
#define RCC_CFGR_SW_PLL      (0x2 << 0)
#define RCC_CFGR_SWS_PLL     (0x2 << 2)

#define RCC_AHB1ENR_GPIOAEN  (1 << 0)
#define RCC_AHB1ENR_GPIOBEN  (1 << 1)
#define RCC_AHB1ENR_GPIOCEN  (1 << 2)
#define RCC_AHB1ENR_OTGFSEN  (1 << 12)

#define RCC_APB1ENR_USART2EN (1 << 17)

/* USB OTG FS */
#define USB_OTG_FS_BASE   0x50000000UL
#define USB_OTG_FS_PMA_BASE (USB_OTG_FS_BASE + 0x1000UL)

typedef struct {
    volatile uint32_t GOTGCTL;
    volatile uint32_t GOTGINT;
    volatile uint32_t GAHBCFG;
    volatile uint32_t GUSBCFG;
    volatile uint32_t GRSTCTL;
    volatile uint32_t GINTSTS;
    volatile uint32_t GINTMSK;
    volatile uint32_t GRXSTSR;
    volatile uint32_t GRXSTSP;
    volatile uint32_t GRXFSIZ;
    volatile uint32_t DIEPTXF0_HNPTXFSIZ;
    volatile uint32_t HNPTXSTS;
    volatile uint32_t RESERVED0[2];
    volatile uint32_t GCCFG;
    volatile uint32_t CID;
    volatile uint32_t RESERVED1[48];
    volatile uint32_t HPTXFSIZ;
    volatile uint32_t DIEPTXF[15];
} USB_OTG_GlobalTypeDef;

typedef struct {
    volatile uint32_t DCFG;
    volatile uint32_t DCTL;
    volatile uint32_t DSTS;
    volatile uint32_t RESERVED0;
    volatile uint32_t DIEPMSK;
    volatile uint32_t RESERVED1;
    volatile uint32_t DOEPMSK;
    volatile uint32_t RESERVED2;
    volatile uint32_t DAINT;
    volatile uint32_t DAINTMSK;
    volatile uint32_t RESERVED3[2];
    volatile uint32_t DVBUSDIS;
    volatile uint32_t DVBUSPULSE;
    volatile uint32_t RESERVED4;
    volatile uint32_t DIEPEMPMSK;
} USB_OTG_DeviceTypeDef;

typedef struct {
    volatile uint32_t DIEPCTL;
    volatile uint32_t RESERVED0;
    volatile uint32_t DIEPINT;
    volatile uint32_t RESERVED1;
    volatile uint32_t DIEPTSIZ;
    volatile uint32_t RESERVED2;
    volatile uint32_t DTXFSTS;
    volatile uint32_t RESERVED3;
} USB_OTG_INEndpointTypeDef;

typedef struct {
    volatile uint32_t DOEPCTL;
    volatile uint32_t RESERVED0;
    volatile uint32_t DOEPINT;
    volatile uint32_t RESERVED1;
    volatile uint32_t DOEPTSIZ;
    volatile uint32_t RESERVED2;
    volatile uint32_t RESERVED3;
    volatile uint32_t RESERVED4;
} USB_OTG_OUTEndpointTypeDef;

#define USB_OTG_FS  ((USB_OTG_GlobalTypeDef *)USB_OTG_FS_BASE)
#define USB_OTG_FS_DEV  ((USB_OTG_DeviceTypeDef *)(USB_OTG_FS_BASE + 0x800))
#define USB_OTG_FS_IN_EP(ep)  ((USB_OTG_INEndpointTypeDef *)(USB_OTG_FS_BASE + 0x900 + (ep) * 0x20))
#define USB_OTG_FS_OUT_EP(ep) ((USB_OTG_OUTEndpointTypeDef *)(USB_OTG_FS_BASE + 0xB00 + (ep) * 0x20))

/* Global register bits */
#define USB_OTG_GINTMSK_USBRST    (1 << 12)
#define USB_OTG_GINTMSK_ENUMDNEM  (1 << 13)
#define USB_OTG_GINTMSK_USBSUSPM  (1 << 11)
#define USB_OTG_GINTMSK_WUIM      (1 << 31)
#define USB_OTG_GINTMSK_RXFLVLM   (1 << 10)
#define USB_OTG_GINTMSK_IEPINT    (1 << 18)
#define USB_OTG_GINTMSK_OEPINT    (1 << 19)
#define USB_OTG_GINTMSK_CTRM      (USB_OTG_GINTMSK_IEPINT | USB_OTG_GINTMSK_OEPINT)

#define USB_OTG_GINTSTS_USBRST    (1 << 12)
#define USB_OTG_GINTSTS_ENUMDNE   (1 << 13)
#define USB_OTG_GINTSTS_USBSUSP   (1 << 11)
#define USB_OTG_GINTSTS_WKUPINT   (1 << 31)
#define USB_OTG_GINTSTS_RXFLVL    (1 << 10)
#define USB_OTG_GINTSTS_IEPINT    (1 << 18)
#define USB_OTG_GINTSTS_OEPINT    (1 << 19)

#define USB_OTG_GUSBCFG_PHYSEL    (1 << 0)  /* Full speed */
#define USB_OTG_GUSBCFG_TRDT_6    (0x5 << 10) /* USB turnaround time */
#define USB_OTG_GUSBCFG_FDMOD     (1 << 30)  /* Force device mode */

#define USB_OTG_GAHBCFG_GINT      (1 << 0)   /* Global interrupt mask */

#define USB_OTG_GRSTCTL_CSRST     (1 << 0)   /* Core soft reset */
#define USB_OTG_GRSTCTL_AHBIDL    (1 << 31)  /* AHB master idle */
#define USB_OTG_GRSTCTL_TXFFLSH   (1 << 5)   /* TX FIFO flush */
#define USB_OTG_GRSTCTL_RXFFLSH   (1 << 4)   /* RX FIFO flush */

#define USB_OTG_GCCFG_PWRDWN      (1 << 16)  /* Power down */
#define USB_OTG_GCCFG_NOVBUSSENS  (1 << 21)  /* No VBUS sensing */

#define USB_OTG_DCFG_DSPD_FS      (0x3 << 0) /* Full speed */
#define USB_OTG_DCFG_NZLSOHSK     (1 << 2)   /* NAK on STALL */

#define USB_OTG_DCTL_SDIS         (1 << 0)   /* Soft disconnect */
#define USB_OTG_DCTL_CGINAK       (1 << 7)   /* Clear global NAK */

#define USB_OTG_DIEPCTL_MPSIZ_64  (64 << 0)
#define USB_OTG_DIEPCTL_USBAEP    (1 << 31)
#define USB_OTG_DIEPCTL_TXFNUM_1  (1 << 22)
#define USB_OTG_DIEPCTL_SD0PID    (1 << 28)
#define USB_OTG_DIEPCTL_SNAK      (1 << 27)
#define USB_OTG_DIEPCTL_CNAK      (1 << 26)
#define USB_OTG_DIEPCTL_EPENA     (1 << 31)
#define USB_OTG_DIEPCTL_EPTYP_CTRL (0x0 << 18)
#define USB_OTG_DIEPCTL_EPTYP_BULK (0x2 << 18)
#define USB_OTG_DIEPCTL_EPTYP_INTR (0x3 << 18)

#define USB_OTG_DOEPCTL_MPSIZ_64  (64 << 0)
#define USB_OTG_DOEPCTL_USBAEP    (1 << 31)
#define USB_OTG_DOEPCTL_SD0PID    (1 << 28)
#define USB_OTG_DOEPCTL_SNAK      (1 << 27)
#define USB_OTG_DOEPCTL_CNAK      (1 << 26)
#define USB_OTG_DOEPCTL_EPENA     (1 << 31)
#define USB_OTG_DOEPCTL_EPTYP_CTRL (0x0 << 18)
#define USB_OTG_DOEPCTL_EPTYP_BULK (0x2 << 18)

#define USB_OTG_DIEPINT_XFRC      (1 << 0)
#define USB_OTG_DIEPINT_EPDISD    (1 << 1)
#define USB_OTG_DIEPINT_TOC       (1 << 3)
#define USB_OTG_DIEPINT_INEPNE    (1 << 6)
#define USB_OTG_DIEPINT_TXFE      (1 << 7)
#define USB_OTG_DIEPINT_NAK       (1 << 13)

#define USB_OTG_DOEPINT_XFRC      (1 << 0)
#define USB_OTG_DOEPINT_STUP      (1 << 5)
#define USB_OTG_DOEPINT_OTEPDIS   (1 << 4)

#define USB_OTG_DIEPTSIZ_STUPCNT_1 (0x1 << 29)
#define USB_OTG_DIEPTSIZ_PKTCNT_1  (1 << 19)
#define USB_OTG_DIEPTSIZ_XFRSIZ_64 (64 << 0)

#define USB_OTG_DOEPTSIZ_STUPCNT_1 (0x1 << 29)
#define USB_OTG_DOEPTSIZ_PKTCNT_1  (1 << 19)
#define USB_OTG_DOEPTSIZ_XFRSIZ_64 (64 << 0)

#define USB_OTG_DTXFSTS_TXFSAV_64 (64 << 0)

/* SysTick */
#define SYSTICK_BASE    0xE000E010UL
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_TypeDef;
#define SYSTICK  ((SysTick_TypeDef *)SYSTICK_BASE)

#define SYSTICK_CTRL_ENABLE   (1 << 0)
#define SYSTICK_CTRL_TICKINT  (1 << 1)
#define SYSTICK_CTRL_CLKSOURCE (1 << 2)

/* NVIC */
#define NVIC_ISER    ((volatile uint32_t *)0xE000E100UL)
#define NVIC_ICER    ((volatile uint32_t *)0xE000E180UL)

/* Flash */
#define FLASH_BASE    0x40023C00UL
typedef struct {
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t OPTCR;
} FLASH_TypeDef;
#define FLASH  ((FLASH_TypeDef *)FLASH_BASE)

#define FLASH_ACR_PRFTEN     (1 << 8)
#define FLASH_ACR_ICEN       (1 << 9)
#define FLASH_ACR_DCEN       (1 << 10)
#define FLASH_ACR_LATENCY_3  (0x3 << 0)

/* ------------------------------------------------------------------ */
/*  GPIO port lookup                                                   */
/* ------------------------------------------------------------------ */

static inline GPIO_TypeDef *stm32_gpio(uint8_t port)
{
    static GPIO_TypeDef *const gpios[] = { GPIOA, GPIOB, GPIOC };
    return gpios[port];
}

/* ------------------------------------------------------------------ */
/*  Interrupts                                                         */
/* ------------------------------------------------------------------ */

static inline void hal_impl_irq_enable(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

static inline void hal_impl_irq_disable(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

/* ------------------------------------------------------------------ */
/*  Delays (simple busy-wait, sufficient for BDM timing)               */
/* ------------------------------------------------------------------ */

static inline void hal_impl_delay_us(uint16_t us)
{
    volatile uint32_t count = us * (F_CPU / 1000000UL);
    while (count--)
        __asm volatile ("nop");
}

/* ------------------------------------------------------------------ */
/*  GPIO                                                               */
/* ------------------------------------------------------------------ */

static inline void hal_impl_gpio_set_output(uint8_t port, uint8_t pin)
{
    GPIO_TypeDef *gpio = stm32_gpio(port);
    uint32_t shift = pin * 2;
    gpio->MODER = (gpio->MODER & ~(0x3 << shift)) | (0x1 << shift);  /* Output */
    gpio->OTYPER &= ~(1 << pin);  /* Push-pull */
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(0x3 << shift)) | (0x1 << shift);  /* Medium speed */
    gpio->PUPDR &= ~(0x3 << shift);  /* No pull-up/pull-down */
}

static inline void hal_impl_gpio_set_input_pullup(uint8_t port, uint8_t pin)
{
    GPIO_TypeDef *gpio = stm32_gpio(port);
    uint32_t shift = pin * 2;
    gpio->MODER = (gpio->MODER & ~(0x3 << shift)) | (0x0 << shift);  /* Input */
    gpio->PUPDR = (gpio->PUPDR & ~(0x3 << shift)) | (0x1 << shift);  /* Pull-up */
}

static inline void hal_impl_gpio_set_high(uint8_t port, uint8_t pin)
{
    stm32_gpio(port)->BSRR = (1 << pin);
}

static inline void hal_impl_gpio_set_low(uint8_t port, uint8_t pin)
{
    stm32_gpio(port)->BSRR = (1 << (pin + 16));
}

static inline uint8_t hal_impl_gpio_read(uint8_t port, uint8_t pin)
{
    return (stm32_gpio(port)->IDR & (1 << pin)) ? 1 : 0;
}

#endif
