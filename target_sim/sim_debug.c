#include <stdarg.h>
#include <string.h>
#include "hal.h"
#include "ringbuf.h"
#include "sim_debug.h"

/* ------------------------------------------------------------------ */
/*  Ring buffer for debug output (separate from USB RX buffer)         */
/* ------------------------------------------------------------------ */

static ringbuf_t dbg_buf;

/* ------------------------------------------------------------------ */
/*  Minimal integer-to-string (no heap, no stdio)                      */
/* ------------------------------------------------------------------ */

static int fmt_u32(char *buf, uint32_t val, unsigned base, int width, char pad)
{
    char tmp[12];
    int i = 0;

    if (val == 0)
        tmp[i++] = '0';
    else {
        while (val > 0) {
            unsigned d = val % base;
            tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
            val /= base;
        }
    }

    int len = i;
    while (i < width)
        tmp[i++] = pad;

    int out = 0;
    while (i > 0)
        buf[out++] = tmp[--i];

    return len;
}

/* ------------------------------------------------------------------ */
/*  Format string into ring buffer (non-blocking, truncates if full)   */
/* ------------------------------------------------------------------ */

static void fmt_enqueue(const char *fmt, va_list args)
{
    const char *p = fmt;
    char numbuf[16];

    while (*p) {
        if (*p != '%') {
            ringbuf_push(&dbg_buf, (uint8_t)*p++);
            continue;
        }

        p++; /* skip '%' */

        /* Parse width and pad */
        int width = 0;
        char pad = ' ';
        if (*p == '0') {
            pad = '0';
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        switch (*p++) {
        case 'u': {
            uint32_t val = va_arg(args, uint32_t);
            int n = fmt_u32(numbuf, val, 10, width, pad);
            for (int j = 0; j < n; j++)
                ringbuf_push(&dbg_buf, (uint8_t)numbuf[j]);
            break;
        }
        case 'x': {
            uint32_t val = va_arg(args, uint32_t);
            if (width == 0) width = 8;
            int n = fmt_u32(numbuf, val, 16, width, pad);
            for (int j = 0; j < n; j++)
                ringbuf_push(&dbg_buf, (uint8_t)numbuf[j]);
            break;
        }
        case 'X': {
            uint32_t val = va_arg(args, uint32_t);
            if (width == 0) width = 8;
            int n = fmt_u32(numbuf, val, 16, width, pad);
            /* Uppercase */
            for (int j = 0; j < n; j++) {
                char c = numbuf[j];
                if (c >= 'a' && c <= 'f')
                    c = c - 'a' + 'A';
                ringbuf_push(&dbg_buf, (uint8_t)c);
            }
            break;
        }
        case 'c': {
            ringbuf_push(&dbg_buf, (uint8_t)va_arg(args, int));
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (s) {
                while (*s)
                    ringbuf_push(&dbg_buf, (uint8_t)*s++);
            }
            break;
        }
        case '%': {
            ringbuf_push(&dbg_buf, (uint8_t)'%');
            break;
        }
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void dbg_init(void)
{
    ringbuf_init(&dbg_buf);
}

void dbg_log(const char *fmt, ...)
{
    if (ringbuf_is_full(&dbg_buf))
        return; /* Drop event — never block */

    va_list args;
    va_start(args, fmt);
    fmt_enqueue(fmt, args);
    va_end(args);
}

void dbg_drain(void)
{
    uint8_t byte;

    /* Send bytes only while USB TX is ready — never block */
    while (ringbuf_pop(&dbg_buf, &byte)) {
        if (!hal_serial_try_putc((char)byte)) {
            /* USB TX busy — push byte back and return */
            ringbuf_push(&dbg_buf, byte);
            return;
        }
    }
}

bool dbg_pending(void)
{
    return !ringbuf_is_empty(&dbg_buf);
}

/* ------------------------------------------------------------------ */
/*  Structured event logging (pre-formatted, faster than dbg_log)      */
/* ------------------------------------------------------------------ */

static const char *event_names[] = {
    "BDM_ENTRY", "BDM_EXIT", "OPCODE", "REG_RD",
    "REG_WR", "MEM_RD", "MEM_WR", "STATUS", "ERROR", "INFO"
};

void dbg_event(dbg_event_t type, uint16_t opcode, uint32_t arg1, uint32_t arg2)
{
    if (ringbuf_is_full(&dbg_buf))
        return;

    const char *name = (type <= DBG_INFO) ? event_names[type] : "???";

    dbg_log("[%s] op=0x%04X a1=0x%08X a2=0x%08X\n", name, opcode, arg1, arg2);
}
