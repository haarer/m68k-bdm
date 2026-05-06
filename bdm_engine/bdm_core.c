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

/* ------------------------------------------------------------------ */
/*  Pin helpers                                                        */
/* ------------------------------------------------------------------ */

static inline void bdm_clock_high(void)
{
    BDMC_PORT |=  (1 << BDMC_BIT);
}

static inline void bdm_clock_low(void)
{
    BDMC_PORT &= ~(1 << BDMC_BIT);
}

static inline void bdm_data_high(void)
{
    BDD_PORT |=  (1 << BDD_BIT);
}

static inline void bdm_data_low(void)
{
    BDD_PORT &= ~(1 << BDD_BIT);
}

static inline bool bdm_read_data(void)
{
    return (BDD_PIN & (1 << BDD_BIT)) != 0;
}

static inline bool bdm_read_ack(void)
{
    return (BDMACK_PIN & (1 << BDMACK_BIT)) != 0;
}

static inline void bdm_set_data_output(void)
{
    BDD_DDR |= (1 << BDD_BIT);
}

static inline void bdm_set_data_input(void)
{
    BDD_DDR &= ~(1 << BDD_BIT);
}

/* ------------------------------------------------------------------ */
/*  16-bit word shift (MSB first, bit 16 = ACK from target)            */
/* ------------------------------------------------------------------ */

bool bdm_shift_word(uint16_t out, uint16_t *in)
{
    uint16_t result = 0;

    for (uint8_t bit = 0; bit < 16; bit++) {
        bdm_clock_low();
        bdm_delay_half_period();

        if (bit < 15) {
            bdm_set_data_output();
            if (out & (1 << (15 - bit)))
                bdm_data_high();
            else
                bdm_data_low();
        } else {
            bdm_set_data_input();
        }

        bdm_clock_high();
        bdm_delay_half_period();

        if (bit == 15) {
            result = bdm_read_data() ? 0x8000U : 0x0000U;
        }
    }

    bdm_set_data_output();

    if (in)
        *in = result;

    return true;
}

/* Byte shift wrapper (shifts as 16-bit word with upper byte zeroed) */
bool bdm_shift_byte(uint8_t out, uint8_t *in)
{
    uint16_t wout = (uint16_t)out;
    uint16_t win = 0;

    if (!bdm_shift_word(wout, &win))
        return false;

    if (in)
        *in = (uint8_t)win;

    return true;
}

/* ------------------------------------------------------------------ */
/*  Preamble                                                           */
/* ------------------------------------------------------------------ */

bool bdm_send_preamble(void)
{
    cli();

    bdm_set_data_output();
    bdm_data_high();
    bdm_clock_low();

    bdm_delay_full_period();

    /* 8 clock cycles with BDD high */
    for (uint8_t i = 0; i < 8; i++) {
        bdm_clock_high();
        bdm_delay_half_period();
        bdm_clock_low();
        bdm_delay_half_period();
    }

    bdm_set_data_input();

    /* Wait for BDMACK with timeout */
    bdm_timeout_start();

    for (uint16_t i = 0; i < 1000; i++) {
        if (bdm_read_ack()) {
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
/*  Status read (after command completes)                              */
/* ------------------------------------------------------------------ */

bool bdm_read_status(uint16_t *status)
{
    uint16_t ack = 0;
    if (!bdm_shift_word(0x0000U, &ack))
        return false;

    if (status)
        *status = ack;

    return true;
}

/* Check if status indicates BERR/AERR error (bit 15 set) */
static inline bool bdm_status_is_error(uint16_t status)
{
    return (status & 0x8000U) != 0;
}

/* ------------------------------------------------------------------ */
/*  Memory READ                                                        */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_memory(uint32_t addr, uint8_t size, uint32_t *data)
{
    uint16_t opcode;
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    /* Build opcode with size */
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

    /* Send opcode */
    bdm_shift_word(opcode, NULL);

    /* Send 32-bit address (MSW first) */
    bdm_shift_word((uint16_t)(addr >> 16), NULL);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), NULL);

    /* Read result data */
    if (size == BDM_SIZE_LONG) {
        uint16_t hi, lo;
        bdm_shift_word(0, &hi);
        bdm_shift_word(0, &lo);
        if (data)
            *data = ((uint32_t)hi << 16) | (uint32_t)lo;
    } else if (size == BDM_SIZE_WORD) {
        uint16_t val = 0;
        bdm_shift_word(0, &val);
        if (data)
            *data = (uint32_t)val;
    } else {
        uint16_t val = 0;
        bdm_shift_word(0, &val);
        if (data)
            *data = (uint32_t)(val & 0xFF);
    }

    /* Read status */
    bdm_read_status(&ack);

    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Memory WRITE                                                       */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_memory(uint32_t addr, uint8_t size, uint32_t data)
{
    uint16_t opcode;
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    /* Build opcode with size */
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

    /* Send opcode */
    bdm_shift_word(opcode, NULL);

    /* Send 32-bit address (MSW first) */
    bdm_shift_word((uint16_t)(addr >> 16), NULL);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), NULL);

    /* Send data */
    if (size == BDM_SIZE_LONG) {
        bdm_shift_word((uint16_t)(data >> 16), NULL);
        bdm_shift_word((uint16_t)(data & 0xFFFFU), NULL);
    } else if (size == BDM_SIZE_WORD) {
        bdm_shift_word((uint16_t)data, NULL);
    } else {
        bdm_shift_word((uint16_t)data, NULL);
    }

    /* Read status */
    bdm_read_status(&ack);

    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Memory DUMP (bulk read, auto-increments address)                   */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_dump_memory(uint32_t addr, uint8_t size, uint8_t count, uint32_t *data)
{
    uint16_t ack = 0;

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

        bdm_shift_word(opcode, NULL);

        /* Read result */
        if (size == BDM_SIZE_LONG) {
            uint16_t hi, lo;
            bdm_shift_word(0, &hi);
            bdm_shift_word(0, &lo);
            if (data)
                data[i] = ((uint32_t)hi << 16) | (uint32_t)lo;
        } else if (size == BDM_SIZE_WORD) {
            uint16_t val = 0;
            bdm_shift_word(0, &val);
            if (data)
                data[i] = (uint32_t)val;
        } else {
            uint16_t val = 0;
            bdm_shift_word(0, &val);
            if (data)
                data[i] = (uint32_t)(val & 0xFF);
        }

        bdm_read_status(&ack);
        if (bdm_status_is_error(ack))
            return BDM_ERR_BERR;
    }

    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Memory FILL (bulk write, auto-increments address)                  */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_fill_memory(uint32_t addr, uint8_t size, uint32_t data, uint8_t count)
{
    uint16_t ack = 0;

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

        bdm_shift_word(opcode, NULL);

        /* Send data */
        if (size == BDM_SIZE_LONG) {
            bdm_shift_word((uint16_t)(data >> 16), NULL);
            bdm_shift_word((uint16_t)(data & 0xFFFFU), NULL);
        } else {
            bdm_shift_word((uint16_t)data, NULL);
        }

        bdm_read_status(&ack);
        if (bdm_status_is_error(ack))
            return BDM_ERR_BERR;
    }

    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Data Register READ (RAREG)                                         */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_data_reg(uint8_t reg, uint32_t *value)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RAREG | BDM_REG_CLASS_DATA | (reg & 0x07);
    bdm_shift_word(opcode, NULL);

    /* Read 32-bit value (MSW first) */
    uint16_t hi = 0, lo = 0;
    bdm_shift_word(0, &hi);
    bdm_shift_word(0, &lo);
    if (value)
        *value = ((uint32_t)hi << 16) | (uint32_t)lo;

    bdm_read_status(&ack);
    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Data Register WRITE (WAREG)                                        */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_data_reg(uint8_t reg, uint32_t value)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WAREG | BDM_REG_CLASS_DATA | (reg & 0x07);
    bdm_shift_word(opcode, NULL);

    /* Send 32-bit value (MSW first) */
    bdm_shift_word((uint16_t)(value >> 16), NULL);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), NULL);

    bdm_read_status(&ack);
    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Address Register READ (RAREG)                                      */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_addr_reg(uint8_t reg, uint32_t *value)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RAREG | BDM_REG_CLASS_ADDR | (reg & 0x07);
    bdm_shift_word(opcode, NULL);

    /* Read 32-bit value (MSW first) */
    uint16_t hi = 0, lo = 0;
    bdm_shift_word(0, &hi);
    bdm_shift_word(0, &lo);
    if (value)
        *value = ((uint32_t)hi << 16) | (uint32_t)lo;

    bdm_read_status(&ack);
    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Address Register WRITE (WAREG)                                     */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_addr_reg(uint8_t reg, uint32_t value)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WAREG | BDM_REG_CLASS_ADDR | (reg & 0x07);
    bdm_shift_word(opcode, NULL);

    /* Send 32-bit value (MSW first) */
    bdm_shift_word((uint16_t)(value >> 16), NULL);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), NULL);

    bdm_read_status(&ack);
    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  System Register READ (RSREG)                                       */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_read_sysreg(uint8_t select, uint32_t *value)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RSREG | (select << 3);
    bdm_shift_word(opcode, NULL);

    /* Read 32-bit value (MSW first) */
    uint16_t hi = 0, lo = 0;
    bdm_shift_word(0, &hi);
    bdm_shift_word(0, &lo);
    if (value)
        *value = ((uint32_t)hi << 16) | (uint32_t)lo;

    bdm_read_status(&ack);
    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  System Register WRITE (WSREG)                                      */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_write_sysreg(uint8_t select, uint32_t value)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WSREG | (select << 3);
    bdm_shift_word(opcode, NULL);

    /* Send 32-bit value (MSW first) */
    bdm_shift_word((uint16_t)(value >> 16), NULL);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), NULL);

    bdm_read_status(&ack);
    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Target Reset (RST)                                                 */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_target_reset(void)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_RST, NULL);

    bdm_read_status(&ack);

    /* Assert hardware reset line */
    TARGET_RESET_PORT &= ~(1 << TARGET_RESET_BIT);
    bdm_delay_us(10);
    TARGET_RESET_PORT |= (1 << TARGET_RESET_BIT);

    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Target Halt (external BKPT)                                        */
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
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_GO, NULL);

    bdm_read_status(&ack);
    in_bdm_mode = false;

    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
    return BDM_OK;
}

/* ------------------------------------------------------------------ */
/*  Call (CALL)                                                        */
/* ------------------------------------------------------------------ */

bdm_result_t bdm_call(uint32_t addr)
{
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_CALL, NULL);

    /* Send 32-bit address (MSW first) */
    bdm_shift_word((uint16_t)(addr >> 16), NULL);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), NULL);

    bdm_read_status(&ack);
    in_bdm_mode = false;

    if (bdm_status_is_error(ack))
        return BDM_ERR_BERR;
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
        /* F-line: check for extension word */
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
    uint16_t ack = 0;

    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_NOP, NULL);
    bdm_read_status(&ack);

    if (bdm_status_is_error(ack))
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
    BDMC_DDR       |=  (1 << BDMC_BIT);
    BDD_DDR        |=  (1 << BDD_BIT);
    BDREQ_DDR      &= ~(1 << BDREQ_BIT);
    BDMACK_DDR     &= ~(1 << BDMACK_BIT);
    TARGET_RESET_DDR |= (1 << TARGET_RESET_BIT);

    BDMC_PORT      &= ~(1 << BDMC_BIT);
    BDD_PORT       &= ~(1 << BDD_BIT);
    TARGET_RESET_PORT |= (1 << TARGET_RESET_BIT);

    in_bdm_mode = false;
    bdm_timing_init();
}
