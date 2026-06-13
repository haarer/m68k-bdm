#include <sys/stat.h>
#include <stdint.h>
#define STM32F411xE
#include "stm32f4xx.h"
#include "uart.h"

extern char _ebss;
extern char _estack;

static char *heap_end;

int _write(int file, char *ptr, int len) {
    (void)file;
    for (int i = 0; i < len; i++) uart_putc(ptr[i]);
    return len;
}

int _read(int file, char *ptr, int len) {
    (void)file;
    for (int i = 0; i < len; i++) {
        int c = uart_getc();
        if (c < 0) { uart_flush(); return i ? i : 0; }
        ptr[i] = (char)c;
    }
    return len;
}

void *_sbrk(int incr) {
    if (!heap_end) heap_end = &_ebss;
    char *prev = heap_end;
    uintptr_t limit = (uintptr_t)&_estack - 1024;
    if ((uintptr_t)(heap_end + incr) > limit) return (void *)-1;
    heap_end += incr;
    return (void *)prev;
}

void _exit(int status) { (void)status; for (;;) __WFI(); }
int  _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int  _getpid(void) { return 1; }
int  _close(int file) { (void)file; return -1; }
int  _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int  _isatty(int file) { (void)file; return 1; }
int  _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
