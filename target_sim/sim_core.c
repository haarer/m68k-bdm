#include <string.h>
#include "board_config.h"
#include "hal.h"
#include "sim_bdm.h"
#include "sim_core.h"

/* ------------------------------------------------------------------ */
/*  Register File                                                      */
/* ------------------------------------------------------------------ */

static uint32_t d_regs[8];    /* D0-D7 */
static uint32_t a_regs[8];    /* A0-A7 */
static uint16_t sr;           /* Status Register */
static uint32_t rpc;          /* Return PC */
static uint32_t pcc;          /* Current PC */
static uint32_t atemp;        /* ATEMP (BDM entry source) */
static uint32_t far;          /* Fault Address Register */
static uint32_t vbr;          /* Vector Base Register */
static uint32_t usp;          /* User Stack Pointer */
static uint32_t ssp;          /* Supervisor Stack Pointer */
static uint8_t  sfc;          /* Source Function Code */
static uint8_t  dfc;          /* Destination Function Code */

/* ------------------------------------------------------------------ */
/*  Memory (4KB RAM — sufficient for HIL testing)                      */
/* ------------------------------------------------------------------ */

#define SIM_MEMORY_SIZE 4096
static uint8_t memory[SIM_MEMORY_SIZE];

/* ------------------------------------------------------------------ */
/*  Internal State                                                     */
/* ------------------------------------------------------------------ */

static bool in_bdm_mode = false;
static uint32_t addr_ptr;     /* Auto-increment address pointer */

/* ------------------------------------------------------------------ */
/*  Status Word Constants                                              */
/* ------------------------------------------------------------------ */

#define STATUS_OK         0xFFFFU
#define STATUS_BERR       0x8001U
#define STATUS_ILLEGAL    0x0001U

/* ------------------------------------------------------------------ */
/*  Opcode Constants                                                   */
/* ------------------------------------------------------------------ */

#define OP_NOP        0x0000U
#define OP_RST        0x0100U
#define OP_CALL       0x0200U
#define OP_GO         0x0300U
#define OP_WAREG      0x4100U
#define OP_RAREG      0x4200U
#define OP_WSREG      0x2400U
#define OP_RSREG      0x2500U
#define OP_WRITE      0x0C00U
#define OP_READ       0x0B00U
#define OP_FILL       0x0E00U
#define OP_DUMP       0x0F00U

#define SIZE_BYTE     0x0000U
#define SIZE_WORD     0x0008U
#define SIZE_LONG     0x0010U

#define REG_CLASS_DATA  0x0000U
#define REG_CLASS_ADDR  0x0004U

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static void mem_write_byte(uint32_t addr, uint8_t val)
{
    memory[addr & (SIM_MEMORY_SIZE - 1)] = val;
}

static uint8_t mem_read_byte(uint32_t addr)
{
    return memory[addr & (SIM_MEMORY_SIZE - 1)];
}

static void mem_write_word(uint32_t addr, uint16_t val)
{
    memory[(addr + 0) & (SIM_MEMORY_SIZE - 1)] = (val >> 8) & 0xFF;
    memory[(addr + 1) & (SIM_MEMORY_SIZE - 1)] = val & 0xFF;
}

static uint16_t mem_read_word(uint32_t addr)
{
    return (uint16_t)memory[(addr + 0) & (SIM_MEMORY_SIZE - 1)] << 8 |
           (uint16_t)memory[(addr + 1) & (SIM_MEMORY_SIZE - 1)];
}

static void mem_write_long(uint32_t addr, uint32_t val)
{
    memory[(addr + 0) & (SIM_MEMORY_SIZE - 1)] = (val >> 24) & 0xFF;
    memory[(addr + 1) & (SIM_MEMORY_SIZE - 1)] = (val >> 16) & 0xFF;
    memory[(addr + 2) & (SIM_MEMORY_SIZE - 1)] = (val >> 8) & 0xFF;
    memory[(addr + 3) & (SIM_MEMORY_SIZE - 1)] = val & 0xFF;
}

static uint32_t mem_read_long(uint32_t addr)
{
    return (uint32_t)memory[(addr + 0) & (SIM_MEMORY_SIZE - 1)] << 24 |
           (uint32_t)memory[(addr + 1) & (SIM_MEMORY_SIZE - 1)] << 16 |
           (uint32_t)memory[(addr + 2) & (SIM_MEMORY_SIZE - 1)] << 8 |
           (uint32_t)memory[(addr + 3) & (SIM_MEMORY_SIZE - 1)];
}

/* ------------------------------------------------------------------ */
/*  BDM Entry Detection                                                */
/* ------------------------------------------------------------------ */

static void check_bdm_entry(void)
{
    if (in_bdm_mode)
        return;

    /* BDM entry: RESET rising edge while BKPT is low */
    static bool reset_was_low = false;
    bool reset_now = sim_bdm_reset_asserted();

    if (reset_was_low && !reset_now) {
        /* RESET just rose — check BKPT */
        if (sim_bdm_bkpt_sampled()) {
            /* Enter BDM mode */
            in_bdm_mode = true;
            atemp = 0x00000002;  /* External BKPT entry source */
            rpc = pcc;           /* Save current PC to RPC */
            sim_bdm_assert_freeze();
        }
    }

    reset_was_low = reset_now;
}

/* ------------------------------------------------------------------ */
/*  Command Execution                                                  */
/* ------------------------------------------------------------------ */

static uint16_t exec_opcode(uint16_t opcode, uint32_t *result_hi, uint32_t *result_lo)
{
    uint16_t status = STATUS_OK;
    *result_hi = 0;
    *result_lo = 0;

    /* Decode opcode */
    uint16_t base = opcode & 0xFF00;
    uint16_t detail = opcode & 0x00FF;

    switch (base) {

    case OP_NOP:
        /* No operation */
        status = STATUS_OK;
        break;

    case OP_RST:
        /* Reset peripherals — no-op in simulator */
        status = STATUS_OK;
        break;

    case OP_GO:
        /* Resume execution — exit BDM mode */
        status = STATUS_OK;
        in_bdm_mode = false;
        sim_bdm_deassert_freeze();
        break;

    case OP_CALL:
        /* Call user code at address (sent in next words) */
        /* Address is sent after opcode — handled by caller */
        status = STATUS_OK;
        in_bdm_mode = false;
        sim_bdm_deassert_freeze();
        break;

    case OP_RAREG:
    case OP_WAREG:
    {
        /* Register access: 0x4100/0x4200 | class | reg */
        uint8_t reg_class = detail & 0x04;
        uint8_t reg_num = detail & 0x03;
        uint32_t *reg = reg_class ? &a_regs[reg_num] : &d_regs[reg_num];

        if (base == OP_RAREG) {
            /* Read register */
            *result_hi = (*reg >> 16) & 0xFFFF;
            *result_lo = *reg & 0xFFFF;
        } else {
            /* Write register — value sent in next words */
            /* Caller handles this */
        }
        break;
    }

    case OP_RSREG:
    case OP_WSREG:
    {
        /* System register access: 0x2400/0x2500 | (select << 3) */
        uint8_t select = (detail >> 3) & 0x0F;

        if (base == OP_RSREG) {
            /* Read system register */
            uint32_t val = 0;
            switch (select) {
            case 0x00: val = rpc; break;
            case 0x01: val = pcc; break;
            case 0x02: val = atemp; break;
            case 0x03: val = far; break;
            case 0x04: val = vbr; break;
            case 0x05: val = (uint32_t)sr; break;
            case 0x06: val = usp; break;
            case 0x07: val = ssp; break;
            case 0x08: val = (uint32_t)sfc; break;
            case 0x09: val = (uint32_t)dfc; break;
            default: status = STATUS_ILLEGAL; break;
            }
            *result_hi = (val >> 16) & 0xFFFF;
            *result_lo = val & 0xFFFF;
        } else {
            /* Write system register — value sent in next words */
            /* Caller handles this */
        }
        break;
    }

    case OP_READ:
    {
        /* Read memory: 0x0B00 | size */
        uint16_t size = detail & 0x0018;
        uint32_t addr = addr_ptr;

        if (size == SIZE_LONG) {
            *result_hi = mem_read_long(addr) >> 16;
            *result_lo = mem_read_long(addr) & 0xFFFF;
            addr_ptr += 4;
        } else if (size == SIZE_WORD) {
            *result_hi = 0;
            *result_lo = mem_read_word(addr);
            addr_ptr += 2;
        } else {
            *result_hi = 0;
            *result_lo = mem_read_byte(addr);
            addr_ptr += 1;
        }
        break;
    }

    case OP_WRITE:
    {
        /* Write memory: 0x0C00 | size */
        /* Data sent in next words — handled by caller */
        break;
    }

    case OP_DUMP:
    {
        /* Dump memory (auto-increment): 0x0F00 | size */
        uint16_t size = detail & 0x0018;
        uint32_t addr = addr_ptr;

        if (size == SIZE_LONG) {
            *result_hi = mem_read_long(addr) >> 16;
            *result_lo = mem_read_long(addr) & 0xFFFF;
            addr_ptr += 4;
        } else if (size == SIZE_WORD) {
            *result_hi = 0;
            *result_lo = mem_read_word(addr);
            addr_ptr += 2;
        } else {
            *result_hi = 0;
            *result_lo = mem_read_byte(addr);
            addr_ptr += 1;
        }
        break;
    }

    case OP_FILL:
    {
        /* Fill memory (auto-increment): 0x0E00 | size */
        /* Data sent in next words — handled by caller */
        break;
    }

    default:
        status = STATUS_ILLEGAL;
        break;
    }

    return status;
}

/* ------------------------------------------------------------------ */
/*  Main Command Loop                                                  */
/* ------------------------------------------------------------------ */

bool sim_core_run(void)
{
    if (!in_bdm_mode) {
        check_bdm_entry();
        return false;
    }

    /* Wait for preamble (BKPT drop) */
    if (!sim_bdm_wait_preamble())
        return in_bdm_mode;

    /* Shift in opcode word */
    uint16_t opcode = sim_bdm_shift_word(0, false);

    /* Decode and execute */
    uint32_t result_hi = 0, result_lo = 0;
    uint16_t status = exec_opcode(opcode, &result_hi, &result_lo);

    /* Handle commands that need additional data words */
    uint16_t base = opcode & 0xFF00;
    uint16_t detail = opcode & 0x00FF;

    switch (base) {

    case OP_CALL:
    {
        /* Read 32-bit address */
        uint16_t addr_hi = sim_bdm_shift_word(0, false);
        uint16_t addr_lo = sim_bdm_shift_word(0, false);
        pcc = ((uint32_t)addr_hi << 16) | addr_lo;
        break;
    }

    case OP_RAREG:
    case OP_RSREG:
    {
        /* Send result data, then status */
        sim_bdm_shift_word(result_hi, false);
        sim_bdm_shift_word(result_lo, false);
        sim_bdm_shift_word(status, true);
        break;
    }

    case OP_WAREG:
    case OP_WSREG:
    {
        /* Read 32-bit value from debugger */
        uint16_t val_hi = sim_bdm_shift_word(0, false);
        uint16_t val_lo = sim_bdm_shift_word(0, false);
        uint32_t val = ((uint32_t)val_hi << 16) | val_lo;

        /* Write to register */
        if (base == OP_WAREG) {
            uint8_t reg_class = detail & 0x04;
            uint8_t reg_num = detail & 0x03;
            if (reg_class)
                a_regs[reg_num] = val;
            else
                d_regs[reg_num] = val;
        } else {
            uint8_t select = (detail >> 3) & 0x0F;
            switch (select) {
            case 0x00: rpc = val; break;
            case 0x01: pcc = val; break;
            case 0x02: atemp = val; break;
            case 0x03: far = val; break;
            case 0x04: vbr = val; break;
            case 0x05: sr = (uint16_t)val; break;
            case 0x06: usp = val; break;
            case 0x07: ssp = val; break;
            case 0x08: sfc = (uint8_t)val; break;
            case 0x09: dfc = (uint8_t)val; break;
            }
        }
        sim_bdm_shift_word(status, true);
        break;
    }

    case OP_READ:
    {
        /* Read 32-bit address */
        uint16_t addr_hi = sim_bdm_shift_word(0, false);
        uint16_t addr_lo = sim_bdm_shift_word(0, false);
        addr_ptr = ((uint32_t)addr_hi << 16) | addr_lo;

        /* Re-execute with address set */
        status = exec_opcode(opcode, &result_hi, &result_lo);

        /* Send result */
        uint16_t size = detail & 0x0018;
        if (size == SIZE_LONG) {
            sim_bdm_shift_word(result_hi, false);
            sim_bdm_shift_word(result_lo, false);
        } else {
            sim_bdm_shift_word(result_lo, false);
        }
        sim_bdm_shift_word(status, true);
        break;
    }

    case OP_WRITE:
    {
        /* Read 32-bit address */
        uint16_t addr_hi = sim_bdm_shift_word(0, false);
        uint16_t addr_lo = sim_bdm_shift_word(0, false);
        addr_ptr = ((uint32_t)addr_hi << 16) | addr_lo;

        /* Read data and write to memory */
        uint16_t size = detail & 0x0018;
        if (size == SIZE_LONG) {
            uint16_t d_hi = sim_bdm_shift_word(0, false);
            uint16_t d_lo = sim_bdm_shift_word(0, false);
            mem_write_long(addr_ptr, ((uint32_t)d_hi << 16) | d_lo);
            addr_ptr += 4;
        } else if (size == SIZE_WORD) {
            uint16_t d = sim_bdm_shift_word(0, false);
            mem_write_word(addr_ptr, d);
            addr_ptr += 2;
        } else {
            uint16_t d = sim_bdm_shift_word(0, false);
            mem_write_byte(addr_ptr, (uint8_t)d);
            addr_ptr += 1;
        }
        sim_bdm_shift_word(status, true);
        break;
    }

    case OP_DUMP:
    {
        /* Send result data, then status */
        uint16_t size = detail & 0x0018;
        if (size == SIZE_LONG) {
            sim_bdm_shift_word(result_hi, false);
            sim_bdm_shift_word(result_lo, false);
        } else {
            sim_bdm_shift_word(result_lo, false);
        }
        sim_bdm_shift_word(status, true);
        break;
    }

    case OP_FILL:
    {
        /* Read data and write to memory */
        uint16_t size = detail & 0x0018;
        if (size == SIZE_LONG) {
            uint16_t d_hi = sim_bdm_shift_word(0, false);
            uint16_t d_lo = sim_bdm_shift_word(0, false);
            mem_write_long(addr_ptr, ((uint32_t)d_hi << 16) | d_lo);
            addr_ptr += 4;
        } else if (size == SIZE_WORD) {
            uint16_t d = sim_bdm_shift_word(0, false);
            mem_write_word(addr_ptr, d);
            addr_ptr += 2;
        } else {
            uint16_t d = sim_bdm_shift_word(0, false);
            mem_write_byte(addr_ptr, (uint8_t)d);
            addr_ptr += 1;
        }
        sim_bdm_shift_word(status, true);
        break;
    }

    default:
        /* NOP, RST, GO — status already sent by exec_opcode */
        sim_bdm_shift_word(status, true);
        break;
    }

    return in_bdm_mode;
}

/* ------------------------------------------------------------------ */
/*  Initialization                                                     */
/* ------------------------------------------------------------------ */

void sim_core_init(void)
{
    /* Initialize registers to known values */
    memset(d_regs, 0, sizeof(d_regs));
    memset(a_regs, 0, sizeof(a_regs));
    sr = 0x2700;  /* Supervisor mode, interrupts masked */
    rpc = 0;
    pcc = 0;
    atemp = 0;
    far = 0;
    vbr = 0;
    usp = 0;
    ssp = 0;
    sfc = 0;
    dfc = 0;

    /* Initialize memory to 0xFF (erased flash pattern) */
    memset(memory, 0xFF, sizeof(memory));

    addr_ptr = 0;
    in_bdm_mode = false;

    /* Initialize BDM pins */
    sim_bdm_init();
}

bool sim_core_in_bdm(void)
{
    return in_bdm_mode;
}

uint32_t sim_core_entry_source(void)
{
    return atemp;
}
