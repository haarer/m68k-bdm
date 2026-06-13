#define STM32F411xE
#include "stm32f4xx.h"
#include "uart.h"
#include "ringbuf.h"

static struct Ringbuffer tx_buf;
static struct Ringbuffer rx_buf;

void USART1_Handler(void) {
    uint32_t sr = USART1->SR;

    if (sr & USART_SR_RXNE) {
        uint8_t c = (uint8_t)USART1->DR;
        if (ringbuffer_free(&rx_buf) > 0) {
            ringbuffer_put_head(&rx_buf, c);
        }
    }

    if ((sr & USART_SR_TXE) && !ringbuffer_empty(&tx_buf)) {
        USART1->DR = ringbuffer_get_tail(&tx_buf);
    }

    if (ringbuffer_empty(&tx_buf)) {
        USART1->CR1 &= ~USART_CR1_TXEIE;
    }
}

void uart_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOB->MODER   &= ~(0x3 << 12 | 0x3 << 14);
    GPIOB->MODER   |=  (0x2 << 12 | 0x2 << 14);
    GPIOB->AFR[0]  &= ~(0xF << 24 | 0xF << 28);
    GPIOB->AFR[0]  |=  (0x7 << 24 | 0x7 << 28);
    GPIOB->OSPEEDR |=  (0x3 << 12 | 0x3 << 14);
    GPIOB->PUPDR   &= ~(0x3 << 12 | 0x3 << 14);

    USART1->BRR = 868;

    ringbuffer_clear(&tx_buf);
    ringbuffer_clear(&rx_buf);

    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

    while (!(USART1->SR & USART_SR_TXE));
}

void uart_putc(char c) {
    while (ringbuffer_free(&tx_buf) == 0);
    ringbuffer_put_head(&tx_buf, (uint8_t)c);
    USART1->CR1 |= USART_CR1_TXEIE;
    if (c == '\n') {
        while (ringbuffer_free(&tx_buf) == 0);
        ringbuffer_put_head(&tx_buf, '\r');
        USART1->CR1 |= USART_CR1_TXEIE;
    }
}

void uart_write(const char *s, int len) {
    for (int i = 0; i < len; i++) {
        while (ringbuffer_free(&tx_buf) == 0);
        ringbuffer_put_head(&tx_buf, (uint8_t)s[i]);
        USART1->CR1 |= USART_CR1_TXEIE;
    }
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

int uart_getc(void) {
    if (ringbuffer_empty(&rx_buf)) return -1;
    return ringbuffer_get_tail(&rx_buf);
}

void uart_putc_raw(char c) {
    while (ringbuffer_free(&tx_buf) == 0);
    ringbuffer_put_head(&tx_buf, c);
    USART1->CR1 |= USART_CR1_TXEIE;
}

void uart_flush(void) {
    while (!ringbuffer_empty(&tx_buf));
    while (USART1->CR1 & USART_CR1_TXEIE);
    while (!(USART1->SR & USART_SR_TC));
}
