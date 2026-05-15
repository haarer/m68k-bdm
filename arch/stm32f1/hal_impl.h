#ifndef HAL_IMPL_H
#define HAL_IMPL_H

#include <stdint.h>
#include "board_config.h"

/* ------------------------------------------------------------------ */
/*  Minimal STM32F1 register definitions (no external CMSIS needed)    */
/* ------------------------------------------------------------------ */

#define PERIPH_BASE     0x40000000UL
#define APB1_BASE       (PERIPH_BASE)
#define APB2_BASE       (PERIPH_BASE + 0x10000UL)
#define AHB_BASE        (PERIPH_BASE + 0x20000UL)

/* GPIO */
#define GPIOA_BASE      (APB2_BASE + 0x0800UL)
#define GPIOB_BASE      (APB2_BASE + 0x0C00UL)
#define GPIOC_BASE      (APB2_BASE + 0x1000UL)

typedef struct {
    volatile uint32_t CRL;
    volatile uint32_t CRH;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t BRR;
    volatile uint32_t LCKR;
} GPIO_TypeDef;

#define GPIOA  ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB  ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC  ((GPIO_TypeDef *)GPIOC_BASE)

/* RCC */
#define RCC_BASE        (AHB_BASE + 0x1000UL)
typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
} RCC_TypeDef;
#define RCC  ((RCC_TypeDef *)RCC_BASE)

#define RCC_CR_PLLRDY        (1 << 25)
#define RCC_CR_PLLON         (1 << 24)
#define RCC_CR_HSERDY        (1 << 17)
#define RCC_CR_HSEON         (1 << 16)
#define RCC_CR_HSIRDY        (1 << 1)
#define RCC_CR_HSION         (1 << 0)

#define RCC_CFGR_PLLMULL9    (0x7 << 18)
#define RCC_CFGR_PLLSRC      (1 << 16)
#define RCC_CFGR_PPRE1_DIV2  (0x4 << 8)
#define RCC_CFGR_SW_PLL      (0x2 << 0)
#define RCC_CFGR_SWS_PLL     (0x2 << 2)

#define RCC_APB2ENR_IOPAEN   (1 << 2)
#define RCC_APB2ENR_IOPBEN   (1 << 3)
#define RCC_APB2ENR_IOPCEN   (1 << 4)
#define RCC_APB2ENR_AFIOEN   (1 << 0)

#define RCC_APB1ENR_USBEN    (1 << 23)

/* USB */
#define USB_BASE        (APB1_BASE + 0x5C00UL)
#define USB_PMA_BASE    0x40006000UL

typedef struct {
    volatile uint16_t EP0R;
    volatile uint16_t RESERVED0;
    volatile uint16_t EP1R;
    volatile uint16_t RESERVED1;
    volatile uint16_t EP2R;
    volatile uint16_t RESERVED2;
    volatile uint16_t EP3R;
    volatile uint16_t RESERVED3;
    volatile uint16_t EP4R;
    volatile uint16_t RESERVED4;
    volatile uint16_t EP5R;
    volatile uint16_t RESERVED5;
    volatile uint16_t EP6R;
    volatile uint16_t RESERVED6;
    volatile uint16_t EP7R;
    volatile uint16_t RESERVED7;
    volatile uint16_t RESERVED8[8];
    volatile uint16_t CNTR;
    volatile uint16_t RESERVED9;
    volatile uint16_t ISTR;
    volatile uint16_t RESERVED10;
    volatile uint16_t FNR;
    volatile uint16_t RESERVED11;
    volatile uint16_t DADDR;
    volatile uint16_t RESERVED12;
    volatile uint16_t BTABLE;
    volatile uint16_t RESERVED13;
} USB_TypeDef;
#define USB  ((USB_TypeDef *)USB_BASE)

#define USB_CNTR_CTRM    (1 << 15)
#define USB_CNTR_RESETM  (1 << 10)
#define USB_CNTR_SUSPM   (1 << 11)
#define USB_CNTR_WKUPM   (1 << 12)
#define USB_CNTR_FSUSP   (1 << 14)
#define USB_CNTR_PDWN    (1 << 1)
#define USB_CNTR_FRES    (1 << 0)

#define USB_ISTR_CTR     (1 << 15)
#define USB_ISTR_RESET   (1 << 10)
#define USB_ISTR_SUSP    (1 << 11)
#define USB_ISTR_WKUP    (1 << 12)
#define USB_ISTR_DIR     (1 << 4)
#define USB_ISTR_EP_ID_MASK 0x0F

#define USB_EP_CTR_RX    (1 << 15)
#define USB_EP_CTR_TX    (1 << 7)
#define USB_EP_DTOG_RX   (1 << 14)
#define USB_EP_DTOG_TX   (1 << 6)
#define USB_EP_STAT_RX_MASK (0x3 << 12)
#define USB_EP_STAT_TX_MASK (0x3 << 4)
#define USB_EP_ADDR_MASK 0x0F
#define USB_EP_TYPE_MASK (0x3 << 9)
#define USB_EP_KIND      (1 << 11)

#define USB_EP_STAT_RX_DISABLED  (0x0 << 12)
#define USB_EP_STAT_RX_STALL     (0x1 << 12)
#define USB_EP_STAT_RX_NAK       (0x2 << 12)
#define USB_EP_STAT_RX_VALID     (0x3 << 12)
#define USB_EP_STAT_TX_DISABLED  (0x0 << 4)
#define USB_EP_STAT_TX_STALL     (0x1 << 4)
#define USB_EP_STAT_TX_NAK       (0x2 << 4)
#define USB_EP_STAT_TX_VALID     (0x3 << 4)

#define USB_EP_TYPE_CONTROL      (0x0 << 9)
#define USB_EP_TYPE_BULK         (0x2 << 9)
#define USB_EP_TYPE_INTERRUPT    (0x3 << 9)

#define USB_DADDR_EF       (1 << 7)

/* PMA access */
#define PMA_ACCESS(addr)  (*(volatile uint16_t *)(USB_PMA_BASE + 2 * (addr)))

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
#define FLASH_BASE    0x40022000UL
typedef struct {
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t AR;
    volatile uint32_t RESERVED;
    volatile uint32_t OBR;
    volatile uint32_t WRPR;
} FLASH_TypeDef;
#define FLASH  ((FLASH_TypeDef *)FLASH_BASE)

#define FLASH_ACR_PRFTBE   (1 << 4)
#define FLASH_ACR_LATENCY_2 (0x2 << 0)

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
/*  Delays (using DWT cycle counter for sub-us accuracy)               */
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
    uint32_t shift = (pin < 8) ? (pin * 4) : ((pin - 8) * 4);
    volatile uint32_t *cr = (pin < 8) ? &gpio->CRL : &gpio->CRH;
    uint32_t reg = *cr;
    reg &= ~(0xF << shift);
    reg |= (0x1 << shift);  /* 0b0001 = 10 MHz push-pull output */
    *cr = reg;
}

static inline void hal_impl_gpio_set_input_pullup(uint8_t port, uint8_t pin)
{
    GPIO_TypeDef *gpio = stm32_gpio(port);
    uint32_t shift = (pin < 8) ? (pin * 4) : ((pin - 8) * 4);
    volatile uint32_t *cr = (pin < 8) ? &gpio->CRL : &gpio->CRH;
    uint32_t reg = *cr;
    reg &= ~(0xF << shift);
    reg |= (0x8 << shift);  /* 0b1000 = input with pull-up/pull-down */
    *cr = reg;
    gpio->ODR |= (1 << pin);  /* enable pull-up */
}

static inline void hal_impl_gpio_set_high(uint8_t port, uint8_t pin)
{
    stm32_gpio(port)->BSRR = (1 << pin);
}

static inline void hal_impl_gpio_set_low(uint8_t port, uint8_t pin)
{
    stm32_gpio(port)->BRR = (1 << pin);
}

static inline uint8_t hal_impl_gpio_read(uint8_t port, uint8_t pin)
{
    return (stm32_gpio(port)->IDR & (1 << pin)) ? 1 : 0;
}

#endif
