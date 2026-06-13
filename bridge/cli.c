#define STM32F411xE
#include "stm32f4xx.h"
#include "cli.h"
#include "uart.h"
#include <stdint.h>
#ifdef USE_STDIO
#include <stdio.h>
#endif

#define LED_PIN 13
#define LINE_BUF_SIZE 64

static char line_buf[LINE_BUF_SIZE];
static uint32_t line_len;
static int led_state;

static void led_on(void) {
    GPIOC->BSRR = (1 << LED_PIN) << 16;
    led_state = 1;
}

static void led_off(void) {
    GPIOC->BSRR = (1 << LED_PIN);
    led_state = 0;
}

static void print_prompt(void) {
    uart_puts("> ");
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static int str_has_prefix(const char *s, const char *prefix) {
    while (*prefix && *s == *prefix) { s++; prefix++; }
    return *prefix == '\0';
}

static void skip_spaces(const char **p) {
    while (**p == ' ') (*p)++;
}

static int parse_int(const char **p) {
    int n = 0;
    while (**p >= '0' && **p <= '9') {
        n = n * 10 + (**p - '0');
        (*p)++;
    }
    return n;
}

void cli_init(void) {
    led_state = 0;
    line_len = 0;
    print_prompt();
}

#ifdef USE_STDIO
static void handle_printf_test(void) {
    printf("hello from printf\n");
    printf("int: %d, hex: %#x, unsigned: %u\n", -42, 0xDEAD, 300);
#ifdef USE_FLOAT
    printf("float: %.2f, pi=%.5f\n", 3.14f, 3.1415926535f);
#endif
}
#endif

static void handle_help(void) {
    uart_puts("available commands:\n");
    uart_puts("  help           show this message\n");
    uart_puts("  hello          print greeting\n");
    uart_puts("  led on         turn LED on\n");
    uart_puts("  led off        turn LED off\n");
    uart_puts("  echo <text>    echo back text\n");
    uart_puts("  ping           ping-pong liveness check\n");
    uart_puts("  echobin <n>    echo back n raw binary bytes\n");
    uart_puts("  pktsend <n>    send n bytes of test pattern\n");
#ifdef USE_STDIO
    uart_puts("  printf_test    test printf via newlib stdio\n");
#endif
}

static void handle_hello(void) {
    uart_puts("hello world\n");
}

static void handle_ping(void) {
    uart_puts("pong\n");
}

static void handle_echobin(const char *args) {
    int n = parse_int(&args);
    uart_puts("ok\n");
    for (int i = 0; i < n; i++) {
        int c;
        while ((c = uart_getc()) < 0);
        uart_write((const char *)&c, 1);
    }
}

static void handle_pktsend(const char *args) {
    int n = parse_int(&args);
    uart_puts("ok\n");
    for (int i = 0; i < n; i++) {
        char c = (char)(i & 0xFF);
        uart_write(&c, 1);
    }
}

void cli_process_line(const char *line) {
    skip_spaces(&line);

    if (str_eq(line, "help")) {
        handle_help();
    } else if (str_eq(line, "hello")) {
        handle_hello();
#ifdef USE_STDIO
    } else if (str_eq(line, "printf_test")) {
        handle_printf_test();
#endif
    } else if (str_eq(line, "ping")) {
        handle_ping();
    } else if (str_has_prefix(line, "echobin")) {
        const char *args = line + 7;
        skip_spaces(&args);
        handle_echobin(args);
    } else if (str_has_prefix(line, "pktsend")) {
        const char *args = line + 7;
        skip_spaces(&args);
        handle_pktsend(args);
    } else if (str_eq(line, "led on")) {
        led_on();
        uart_puts("ok\n");
    } else if (str_eq(line, "led off")) {
        led_off();
        uart_puts("ok\n");
    } else if (str_has_prefix(line, "echo")) {
        const char *text = line + 4;
        skip_spaces(&text);
        uart_puts(text);
        uart_putc('\n');
    } else if (*line) {
        uart_puts("error: unknown command\n");
    }

    print_prompt();
}

void cli_poll(void) {
    int c = uart_getc();
    if (c < 0) return;

    if (c == '\r') c = '\n';

    if (c == '\n') {
        uart_putc('\n');
        line_buf[line_len] = '\0';
        cli_process_line(line_buf);
        line_len = 0;
        return;
    }

    if ((c == '\b' || c == 127) && line_len > 0) {
        uart_puts("\b \b");
        line_len--;
        return;
    }

    if (line_len < LINE_BUF_SIZE - 1) {
        line_buf[line_len++] = (char)c;
        uart_putc((char)c);
    }
}
