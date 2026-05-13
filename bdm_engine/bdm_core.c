#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/delay.h>
#include "config.h"
#include "bdm_timing.h"
#include "bdm_core.h"

/* ------------------------------------------------------------------ */
/*  Internal state                                                     */
/* ------------------------------------------------------------------ */

static bool in_bdm_mode = false;
static uint16_t last_status_word = 0;

/* ------------------------------------------------------------------ */
/*  Status helpers                                                     */
/* ------------------------------------------------------------------ */

static inline bool bdm_status_is_error(uint16_t status)
{
    return (status == (uint16_t)BDM_STATUS_BERR ||
            status == (uint16_t)BDM_STATUS_ILLEGAL);
}

/* ------------------------------------------------------------------ */
/*  Pin helpers (CPU32 §7.2.7)                                         */
/* ------------------------------------------------------------------ */

static inline void dsclk_high(void)
{
    DSCLK_PORT |=  (1 << DSCLK_BIT);
}

static inline void dsclk_low(void)
{
    DSCLK_PORT &= ~(1 << DSCLK_BIT);
}

static inline void dsi_high(void)
{
    DSI_PORT |=  (1 << DSI_BIT);
}

static inline void dsi_low(void)
{
    DSI_PORT &= ~(1 << DSI_BIT);
}

static inline bool dso_read(void)
{
    return (DSO_PIN & (1 << DSO_BIT)) != 0;
}

static inline bool freeze_read(void)
{
    return (FREEZE_PIN & (1 << FREEZE_BIT)) != 0;
}

/* ------------------------------------------------------------------ */
/*  17-bit full-duplex word shift (CPU32 §7.2.7)                       */
/*      16 data bits + 1 status bit (bit 16) on DSO.                  */
/*      Data transitions on falling edge of DSCLK,                    */
/*      stable by rising edge, latched on rising edge.                */
/*      MSB first.                                                    */
/* ------------------------------------------------------------------ */

uint16_t bdm_shift_word(uint16_t out, bool poll)
{
    cli();

    uint16_t data_in  = 0;
    uint8_t  status16 = 0;

    for (uint8_t bit = 0; bit < 17; bit++) {
        /* Data transitions on falling edge: set DSI before clock goes low */
        if (bit < 16) {
            if (out & (1 << (15 - bit)))
                dsi_high();
            else
                dsi_low();
        }

        dsclk_low();
        bdm_delay_half_period();

        /* Rising edge: CPU latches DSI, we sample DSO */
        dsclk_high();
        bdm_delay_half_period();

        if (bit < 16) {
            data_in |= (uint16_t)(dso_read() << (15 - bit));
        } else {
            /* Bit 16 = status/control from DSO */
            status16 = dso_read() ? 1U : 0U;
        }
    }

    /* Per spec: DSCLK remains high between transfers */

    if (poll) {
        if (status16 == BDM_STATUS_READY) {
            last_status_word = data_in;
        }
        sei();
        return (status16 == BDM_STATUS_READY) ? data_in : 0xFFFFU;
    }

    sei();
    return data_in;
}

/* ------------------------------------------------------------------ */
/*  Poll until CPU ready (DSO bit 16 = 0)                              */
/*      CPU holds DSO high ("not ready") until response is valid.      */
/*      We send zero words and clock until DSO bit 16 goes low.        */
/*      The 16-bit data word accompanying bit16=0 is the status word.  */
/* ------------------------------------------------------------------ */

bool bdm_poll_ready(void)
{
    bdm_timeout_start();

    for (uint16_t i = 0; i < 10000; i++) {
        cli();
        uint16_t data_in  = 0;
        uint8_t  status16 = 0;

        for (uint8_t bit = 0; bit < 17; bit++) {
            dsi_low();
            dsclk_low();
            bdm_delay_half_period();
            dsclk_high();
            bdm_delay_half_period();
            if (bit < 16)
                data_in |= (uint16_t)(dso_read() << (15 - bit));
            else
                status16 = dso_read() ? 1U : 0U;
        }
        sei();

        if (status16 == BDM_STATUS_READY) {
            last_status_word = data_in;
            return true;
        }
        if (bdm_timeout_exceeded())
            break;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/*  BDM Enable (CPU32 §7.2.1)                                          */
/*      One-time: assert BKPT low, toggle RESET so BKPT samples low   */
/*      at the rising edge of RESET. BKPT must be held low for at     */
/*      least two target clock cycles prior to negation of RESET.     */
/* ------------------------------------------------------------------ */

bool bdm_enable(void)
{
    cli();

    /* 1. Assert BKPT (DSCLK low) */
    dsclk_low();
    dsi_high();

    /* 2. Assert target RESET (active low) */
    TARGET_RESET_PORT &= ~(1 << TARGET_RESET_BIT);

    /* 3. Hold BKPT low for ≥2 target clock cycles before releasing RESET */
    _delay_us(10);

    /* 4. Release RESET — BKPT samples low at rising edge → BDM enabled */
    TARGET_RESET_PORT |= (1 << TARGET_RESET_BIT);

    /* 5. Wait for FREEZE assertion */
    bdm_timeout_start();

    for (uint16_t i = 0; i < 1000; i++) {
        if (!freeze_read()) {
            sei();
            in_bdm_mode = true;
            return true;
        }
        if (bdm_timeout_exceeded())
            break;
        _delay_us(10);
    }

    sei();
    in_bdm_mode = false;
    return false;
}

/* ------------------------------------------------------------------ */
/*  Preamble (CPU32 §7.2.3, §7.2.7.2)                                 */
/*      Per-command: drop BKPT, wait FREEZE, then clock command.      */
/* ------------------------------------------------------------------ */

static bool bdm_send_preamble(void)
{
    cli();

    dsclk_low();
    dsi_high();

    bdm_delay_full_period();

    bdm_timeout_start();

    for (uint16_t i = 0; i < 1000; i++) {
        if (!freeze_read()) {
            sei();
            return true;
        }
        if (bdm_timeout_exceeded())
            break;
        _delay_us(10);
    }

    sei();
    return false;
}

/* ------------------------------------------------------------------ */
/*  Status check after command                                         */
/*      After sending a command, poll for ready. The 16-bit data       */
/*      word accompanying bit16=0 is the status word from the CPU.     */
/* ------------------------------------------------------------------ */

static bool bdm_check_status(void)
{
    if (!bdm_poll_ready())
        return false;

    return !bdm_status_is_error(last_status_word);
}

/* ------------------------------------------------------------------ */
/*  Memory READ                                                        */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_memory(uint32_t addr, uint8_t size, uint32_t *data)
{
    uint16_t opcode;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    switch (size) {
    case BDM_SIZE_WORD:
        opcode = BDM_OPCODE_READ | BDM_OP_SIZE_WORD;
        break;
    case BDM_SIZE_LONG:
        opcode = BDM_OPCODE_READ | BDM_OP_SIZE_LONG;
        break;
    case BDM_SIZE_BYTE:
    default:
        opcode = BDM_OPCODE_READ | BDM_OP_SIZE_BYTE;
        break;
    }

    /* Send opcode word */
    bdm_shift_word(opcode, false);

    /* Send 32-bit address (MSW first) */
    bdm_shift_word((uint16_t)(addr >> 16), false);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), false);

    /* Poll for CPU ready, then read result data */
    if (!bdm_poll_ready())
        return BDM_ERR_TIMEOUT;

    if (size == BDM_SIZE_LONG) {
        uint16_t hi = bdm_shift_word(0, false);
        uint16_t lo = bdm_shift_word(0, false);
        if (data)
            *data = ((uint32_t)hi << 16) | (uint32_t)lo;
    } else if (size == BDM_SIZE_WORD) {
        uint16_t val = bdm_shift_word(0, false);
        if (data)
            *data = (uint32_t)val;
    } else {
        uint16_t val = bdm_shift_word(0, false);
        if (data)
            *data = (uint32_t)(val & 0xFF);
    }

    /* Check status */
    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Memory WRITE                                                       */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_memory(uint32_t addr, uint8_t size, uint32_t data)
{
    uint16_t opcode;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    switch (size) {
    case BDM_SIZE_WORD:
        opcode = BDM_OPCODE_WRITE | BDM_OP_SIZE_WORD;
        break;
    case BDM_SIZE_LONG:
        opcode = BDM_OPCODE_WRITE | BDM_OP_SIZE_LONG;
        break;
    case BDM_SIZE_BYTE:
    default:
        opcode = BDM_OPCODE_WRITE | BDM_OP_SIZE_BYTE;
        break;
    }

    /* Send opcode word */
    bdm_shift_word(opcode, false);

    /* Send 32-bit address (MSW first) */
    bdm_shift_word((uint16_t)(addr >> 16), false);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), false);

    /* Send data */
    if (size == BDM_SIZE_LONG) {
        bdm_shift_word((uint16_t)(data >> 16), false);
        bdm_shift_word((uint16_t)(data & 0xFFFFU), false);
    } else if (size == BDM_SIZE_WORD) {
        bdm_shift_word((uint16_t)data, false);
    } else {
        bdm_shift_word((uint16_t)data, false);
    }

    /* Check status */
    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Memory DUMP (bulk read, auto-increments address)                   */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_dump_memory(uint32_t addr, uint8_t size, uint8_t count, uint32_t *data)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    /* First do a READ to set the address pointer */
    uint32_t dummy;
    bdm_result_t res = bdm_read_memory(addr, size, &dummy);
    if (res != BDM_OK)
        return res;

    /* Then DUMP for remaining count-1 */
    for (uint8_t i = 1; i < count; i++) {
        if (!bdm_send_preamble())
            return BDM_ERR_NO_TARGET;

        uint16_t opcode;
        switch (size) {
        case BDM_SIZE_WORD:
            opcode = BDM_OPCODE_DUMP | BDM_OP_SIZE_WORD;
            break;
        case BDM_SIZE_LONG:
            opcode = BDM_OPCODE_DUMP | BDM_OP_SIZE_LONG;
            break;
        case BDM_SIZE_BYTE:
        default:
            opcode = BDM_OPCODE_DUMP | BDM_OP_SIZE_BYTE;
            break;
        }

        bdm_shift_word(opcode, false);

        /* Poll for ready, then read result */
        if (!bdm_poll_ready())
            return BDM_ERR_TIMEOUT;

        if (size == BDM_SIZE_LONG) {
            uint16_t hi = bdm_shift_word(0, false);
            uint16_t lo = bdm_shift_word(0, false);
            if (data)
                data[i] = ((uint32_t)hi << 16) | (uint32_t)lo;
        } else if (size == BDM_SIZE_WORD) {
            uint16_t val = bdm_shift_word(0, false);
            if (data)
                data[i] = (uint32_t)val;
        } else {
            uint16_t val = bdm_shift_word(0, false);
            if (data)
                data[i] = (uint32_t)(val & 0xFF);
        }

        if (!bdm_check_status())
            return BDM_ERR_BERR;
    }

    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Memory FILL (bulk write, auto-increments address)                  */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_fill_memory(uint32_t addr, uint8_t size, uint32_t data, uint8_t count)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    /* First do a WRITE to set the address pointer */
    bdm_result_t res = bdm_write_memory(addr, size, data);
    if (res != BDM_OK)
        return res;

    /* Then FILL for remaining count-1 */
    for (uint8_t i = 1; i < count; i++) {
        if (!bdm_send_preamble())
            return BDM_ERR_NO_TARGET;

        uint16_t opcode;
        switch (size) {
        case BDM_SIZE_WORD:
            opcode = BDM_OPCODE_FILL | BDM_OP_SIZE_WORD;
            break;
        case BDM_SIZE_LONG:
            opcode = BDM_OPCODE_FILL | BDM_OP_SIZE_LONG;
            break;
        case BDM_SIZE_BYTE:
        default:
            opcode = BDM_OPCODE_FILL | BDM_OP_SIZE_BYTE;
            break;
        }

        bdm_shift_word(opcode, false);

        /* Send data */
        if (size == BDM_SIZE_LONG) {
            bdm_shift_word((uint16_t)(data >> 16), false);
            bdm_shift_word((uint16_t)(data & 0xFFFFU), false);
        } else {
            bdm_shift_word((uint16_t)data, false);
        }

        if (!bdm_check_status())
            return BDM_ERR_BERR;
    }

    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Data Register READ (RAREG)                                         */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_data_reg(uint8_t reg, uint32_t *value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RAREG | BDM_REG_CLASS_DATA | (reg & 0x07);
    bdm_shift_word(opcode, false);

    /* Poll for ready, then read 32-bit value (MSW first) */
    if (!bdm_poll_ready())
        return BDM_ERR_TIMEOUT;

    uint16_t hi = bdm_shift_word(0, false);
    uint16_t lo = bdm_shift_word(0, false);
    if (value)
        *value = ((uint32_t)hi << 16) | (uint32_t)lo;

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Data Register WRITE (WAREG)                                        */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_data_reg(uint8_t reg, uint32_t value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WAREG | BDM_REG_CLASS_DATA | (reg & 0x07);
    bdm_shift_word(opcode, false);

    /* Send 32-bit value (MSW first) */
    bdm_shift_word((uint16_t)(value >> 16), false);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Address Register READ (RAREG)                                      */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_addr_reg(uint8_t reg, uint32_t *value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RAREG | BDM_REG_CLASS_ADDR | (reg & 0x07);
    bdm_shift_word(opcode, false);

    /* Poll for ready, then read 32-bit value (MSW first) */
    if (!bdm_poll_ready())
        return BDM_ERR_TIMEOUT;

    uint16_t hi = bdm_shift_word(0, false);
    uint16_t lo = bdm_shift_word(0, false);
    if (value)
        *value = ((uint32_t)hi << 16) | (uint32_t)lo;

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Address Register WRITE (WAREG)                                     */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_addr_reg(uint8_t reg, uint32_t value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WAREG | BDM_REG_CLASS_ADDR | (reg & 0x07);
    bdm_shift_word(opcode, false);

    /* Send 32-bit value (MSW first) */
    bdm_shift_word((uint16_t)(value >> 16), false);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  System Register READ (RSREG)                                       */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_sysreg(uint8_t select, uint32_t *value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RSREG | (select << 3);
    bdm_shift_word(opcode, false);

    /* Poll for ready, then read 32-bit value (MSW first) */
    if (!bdm_poll_ready())
        return BDM_ERR_TIMEOUT;

    uint16_t hi = bdm_shift_word(0, false);
    uint16_t lo = bdm_shift_word(0, false);
    if (value)
        *value = ((uint32_t)hi << 16) | (uint32_t)lo;

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  System Register WRITE (WSREG)                                      */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_sysreg(uint8_t select, uint32_t value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WSREG | (select << 3);
    bdm_shift_word(opcode, false);

    /* Send 32-bit value (MSW first) */
    bdm_shift_word((uint16_t)(value >> 16), false);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Target Reset (RST)                                                 */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_target_reset(void)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_RST, false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;

    /* Assert hardware reset line */
    TARGET_RESET_PORT &= ~(1 << TARGET_RESET_BIT);
    bdm_delay_us(10);
    TARGET_RESET_PORT |= (1 << TARGET_RESET_BIT);

    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Target Halt (via BKPT assertion)                                   */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_target_halt(void)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    /* Read ATEMP to determine entry source */
    uint32_t atemp;
    bdm_read_sysreg(BDM_SR_ATEMP, &atemp);

    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Target Go (GO)                                                     */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_target_go(void)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_GO, false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;

    in_bdm_mode = false;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Call (CALL)                                                        */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_call(uint32_t addr)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_CALL, false);

    /* Send 32-bit address (MSW first) */
    bdm_shift_word((uint16_t)(addr >> 16), false);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;

    in_bdm_mode = false;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Step (set breakpoint at PC+instr, GO, wait)                        */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_step(void)
{
    uint32_t pc;

    /* Read current PC via RSREG (PCC) */
    bdm_result_t res = bdm_read_sysreg(BDM_SR_PCC, &pc);
    if (res != BDM_OK)
        return res;

    /* Read instruction word to determine length */
    uint32_t instr;
    res = bdm_read_memory(pc, BDM_SIZE_WORD, &instr);
    if (res != BDM_OK)
        return res;

    /* Determine instruction length (simplified: assume 2 bytes minimum,
       check for extension words) */
    uint16_t instr_len = 2;

    /* Check for extension word (bits [15:12] == 1111 for F-line,
       or indexed addressing mode) */
    uint16_t opcode = (uint16_t)(instr & 0xFFFF);
    if ((opcode & 0xF000) == 0xF000) {
        uint32_t ext;
        res = bdm_read_memory(pc + 2, BDM_SIZE_WORD, &ext);
        if (res == BDM_OK) {
            uint16_t extw = (uint16_t)(ext & 0xFFFF);
            if (extw & 0x8000) {
                instr_len += 2;
            }
        }
    }

    /* Compute next PC */
    uint32_t next_pc = pc + instr_len;

    /* For now, just advance PC via WSREG and return -- full
       breakpoint-based stepping requires breakpoint hardware support */
    bdm_write_sysreg(BDM_SR_RPC, next_pc);

    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  NOP                                                                */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_nop(void)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_NOP, false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  State query                                                        */
/* ------------------------------------------------------------------ */

bool bdm_in_bdm_mode(void)
{
    return in_bdm_mode;
}

/* ------------------------------------------------------------------ */
/*  Initialization                                                     */
/* ------------------------------------------------------------------ */

void bdm_init(void)
{
    DSCLK_DDR      |=  (1 << DSCLK_BIT);
    DSI_DDR        |=  (1 << DSI_BIT);
    FREEZE_DDR     &= ~(1 << FREEZE_BIT);
    DSO_DDR        &= ~(1 << DSO_BIT);
    TARGET_RESET_DDR |= (1 << TARGET_RESET_BIT);

    DSCLK_PORT     |=  (1 << DSCLK_BIT);
    DSI_PORT       &= ~(1 << DSI_BIT);
    TARGET_RESET_PORT |= (1 << TARGET_RESET_BIT);

    /* Enable pull-ups on input pins so they read high (no target)
       when nothing is connected. */
    FREEZE_PORT    |=  (1 << FREEZE_BIT);
    DSO_PORT       |=  (1 << DSO_BIT);

    in_bdm_mode = false;
    bdm_timing_init();
}
