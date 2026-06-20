#define STM32F411xE
#include "stm32f4xx.h"
#include "cli.h"
#include "uart.h"
#include "bdm_core.h"
#include <stdint.h>

#define LED_PIN 13
#define LINE_BUF_SIZE 80

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
    uart_puts("bdm> ");
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

static int parse_hex(const char **p) {
    int n = 0;
    skip_spaces(p);
    if (**p == '0' && (*p)[1] == 'x') {
        *p += 2;
        while ((**p >= '0' && **p <= '9') || (**p >= 'a' && **p <= 'f') || (**p >= 'A' && **p <= 'F')) {
            n = n * 16;
            if (**p >= '0' && **p <= '9') n += **p - '0';
            else if (**p >= 'a' && **p <= 'f') n += **p - 'a' + 10;
            else n += **p - 'A' + 10;
            (*p)++;
        }
    } else {
        while (**p >= '0' && **p <= '9') {
            n = n * 10 + (**p - '0');
            (*p)++;
        }
    }
    return n;
}

static void uart_put_u32(uint32_t val) {
    char buf[11];
    int i = 10;
    buf[10] = '\0';
    for (; i > 0; i--) {
        buf[i - 1] = "0123456789ABCDEF"[val & 0xF];
        val >>= 4;
    }
    uart_puts(buf + 2);
    uart_puts("x");
    uart_putc('0');
}

static void uart_put_dec(uint32_t val) {
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    do {
        buf[--i] = '0' + (val % 10);
        val /= 10;
    } while (val);
    uart_puts(buf + i);
}

static void uart_put_result(const char *name, int ok) {
    uart_puts(name);
    uart_puts(ok ? " [OK]\n" : " [FAIL]\n");
}

void cli_init(void) {
    led_state = 0;
    line_len = 0;
    print_prompt();
}

static void handle_help(void) {
    uart_puts("BDM CLI commands:\n");
    uart_puts("  help              show this message\n");
    uart_puts("  hello             print greeting\n");
    uart_puts("  led on/off        control LED\n");
    uart_puts("  enable            BDM enable reset sequence\n");
    uart_puts("  status            query BDM mode\n");
    uart_puts("  halt              halt target\n");
    uart_puts("  go                resume target\n");
    uart_puts("  reset             target reset\n");
    uart_puts("  step              single-step\n");
    uart_puts("  mread ADDR [SIZE] read memory (1/2/4)\n");
    uart_puts("  mwrite ADDR VAL   write memory\n");
    uart_puts("  regread REG       read D/A register (0-15)\n");
    uart_puts("  regwrite REG VAL  write D/A register\n");
    uart_puts("  sysreg SEL        read system register\n");
    uart_puts("  syswr SEL VAL     write system register\n");
    uart_puts("  call ADDR         call code at address\n");
    uart_puts("  nop               BDM no-op\n");
}

static void handle_hello(void) {
    uart_puts("hello world\n");
}

static void handle_enable(void) {
    int ok = bdm_enable();
    uart_put_result("BDM enable", ok);
}

static void handle_status(void) {
    if (bdm_in_bdm_mode())
        uart_puts("Target in BDM mode [OK]\n");
    else
        uart_puts("Target in normal mode\n");
}

static void handle_halt(void) {
    bdm_result_t r = bdm_target_halt();
    uart_put_result("Halt", r == BDM_OK);
}

static void handle_go(void) {
    bdm_result_t r = bdm_target_go();
    uart_put_result("Go", r == BDM_OK);
}

static void handle_reset(void) {
    bdm_result_t r = bdm_target_reset();
    uart_put_result("Reset", r == BDM_OK);
}

static void handle_step(void) {
    bdm_result_t r = bdm_step();
    uart_put_result("Step", r == BDM_OK);
}

static void handle_nop(void) {
    bdm_result_t r = bdm_nop();
    uart_put_result("NOP", r == BDM_OK);
}

static void handle_mread(const char *args) {
    uint32_t addr = parse_hex(&args);
    skip_spaces(&args);
    uint8_t size = 1;
    if (*args) {
        int s = parse_hex(&args);
        if (s == 2 || s == 4) size = s;
    }
    uint32_t data;
    bdm_result_t r = bdm_read_memory(addr, size, &data);
    if (r == BDM_OK) {
        uart_puts("mread 0x");
        uart_put_u32(addr);
        uart_puts(" = 0x");
        uart_put_u32(data);
        uart_puts(" [OK]\n");
    } else {
        uart_puts("mread [ERROR]\n");
    }
}

static void handle_mwrite(const char *args) {
    uint32_t addr = parse_hex(&args);
    skip_spaces(&args);
    uint32_t data = parse_hex(&args);
    skip_spaces(&args);
    uint8_t size = 4;
    if (*args) {
        int s = parse_hex(&args);
        if (s == 1 || s == 2) size = s;
    }
    bdm_result_t r = bdm_write_memory(addr, size, data);
    uart_put_result("mwrite", r == BDM_OK);
}

static void handle_regread(const char *args) {
    int reg = parse_hex(&args);
    uint32_t val;
    bdm_result_t r;
    if (reg < 8)
        r = bdm_read_data_reg(reg, &val);
    else
        r = bdm_read_addr_reg(reg - 8, &val);
    if (r == BDM_OK) {
        if (reg < 8) { uart_puts("D"); uart_put_dec(reg); }
        else { uart_puts("A"); uart_put_dec(reg - 8); }
        uart_puts(" = 0x");
        uart_put_u32(val);
        uart_puts(" [OK]\n");
    } else {
        uart_puts("regread [ERROR]\n");
    }
}

static void handle_regwrite(const char *args) {
    int reg = parse_hex(&args);
    skip_spaces(&args);
    uint32_t val = parse_hex(&args);
    bdm_result_t r;
    if (reg < 8)
        r = bdm_write_data_reg(reg, val);
    else
        r = bdm_write_addr_reg(reg - 8, val);
    uart_put_result("regwrite", r == BDM_OK);
}

static void handle_sysreg(const char *args) {
    int sel = parse_hex(&args);
    uint32_t val;
    bdm_result_t r = bdm_read_sysreg(sel, &val);
    if (r == BDM_OK) {
        uart_puts("SYSREG[0x");
        uart_put_u32(sel);
        uart_puts("] = 0x");
        uart_put_u32(val);
        uart_puts(" [OK]\n");
    } else {
        uart_puts("sysreg [ERROR]\n");
    }
}

static void handle_syswr(const char *args) {
    int sel = parse_hex(&args);
    skip_spaces(&args);
    uint32_t val = parse_hex(&args);
    bdm_result_t r = bdm_write_sysreg(sel, val);
    uart_put_result("syswr", r == BDM_OK);
}

static void handle_call(const char *args) {
    uint32_t addr = parse_hex(&args);
    bdm_result_t r = bdm_call(addr);
    uart_put_result("call", r == BDM_OK);
}

void cli_process_line(const char *line) {
    skip_spaces(&line);

    if (str_eq(line, "help") || str_eq(line, "?"))
        handle_help();
    else if (str_eq(line, "hello"))
        handle_hello();
    else if (str_eq(line, "led on")) { led_on(); uart_puts("ok\n"); }
    else if (str_eq(line, "led off")) { led_off(); uart_puts("ok\n"); }
    else if (str_has_prefix(line, "enable"))
        handle_enable();
    else if (str_has_prefix(line, "status"))
        handle_status();
    else if (str_has_prefix(line, "halt"))
        handle_halt();
    else if (str_has_prefix(line, "go"))
        handle_go();
    else if (str_has_prefix(line, "reset"))
        handle_reset();
    else if (str_has_prefix(line, "step"))
        handle_step();
    else if (str_has_prefix(line, "nop"))
        handle_nop();
    else if (str_has_prefix(line, "mread"))
        handle_mread(line + 5);
    else if (str_has_prefix(line, "mwrite"))
        handle_mwrite(line + 6);
    else if (str_has_prefix(line, "regread"))
        handle_regread(line + 7);
    else if (str_has_prefix(line, "regwrite"))
        handle_regwrite(line + 8);
    else if (str_has_prefix(line, "syswr"))
        handle_syswr(line + 5);
    else if (str_has_prefix(line, "sysreg"))
        handle_sysreg(line + 6);
    else if (str_has_prefix(line, "call"))
        handle_call(line + 4);
    else if (str_has_prefix(line, "echo")) {
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
