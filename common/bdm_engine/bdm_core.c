#include "config.h"
#include "bdm_timing.h"
#include "bdm_core.h"
#include "bdm_pins.h"
#include "delay.h"

static bool in_bdm_mode = false;
static uint16_t last_status_word = 0;

static inline bool bdm_status_is_error(uint16_t status)
{
    return (status == BDM_STATUS_BERR ||
            status == BDM_STATUS_ILLEGAL);
}

static inline void dsclk_high(void) { bdm_gpio_set(BDM_DSCLK_GPIO, BDM_DSCLK_PIN); }
static inline void dsclk_low(void)  { bdm_gpio_clr(BDM_DSCLK_GPIO, BDM_DSCLK_PIN); }
static inline void dsi_high(void)   { bdm_gpio_set(BDM_DSI_GPIO, BDM_DSI_PIN); }
static inline void dsi_low(void)    { bdm_gpio_clr(BDM_DSI_GPIO, BDM_DSI_PIN); }
static inline bool dso_read(void)   { return bdm_gpio_read(BDM_DSO_GPIO, BDM_DSO_PIN); }
static inline bool freeze_read(void){ return bdm_gpio_read(BDM_FREEZE_GPIO, BDM_FREEZE_PIN); }

static inline void target_reset_low(void)  { bdm_gpio_clr(BDM_RESET_GPIO, BDM_RESET_PIN); }
static inline void target_reset_high(void) { bdm_gpio_set(BDM_RESET_GPIO, BDM_RESET_PIN); }

uint16_t bdm_shift_word(uint16_t out, bool poll)
{
    __disable_irq();

    uint16_t data_in  = 0;
    uint8_t  status16 = 0;

    for (uint8_t bit = 0; bit < 17; bit++) {
        if (bit < 16) {
            if (out & (1 << (15 - bit)))
                dsi_high();
            else
                dsi_low();
        }

        dsclk_low();
        bdm_delay_half_period();

        dsclk_high();
        bdm_delay_half_period();

        if (bit < 16) {
            data_in |= (uint16_t)(dso_read() << (15 - bit));
        } else {
            status16 = dso_read() ? 1U : 0U;
        }
    }

    if (poll) {
        if (status16 == BDM_STATUS_READY) {
            last_status_word = data_in;
        }
        __enable_irq();
        return (status16 == BDM_STATUS_READY) ? data_in : 0xFFFFU;
    }

    __enable_irq();
    return data_in;
}

bool bdm_poll_ready(void)
{
    bdm_timeout_start();

    for (uint16_t i = 0; i < 10000; i++) {
        __disable_irq();
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
        __enable_irq();

        if (status16 == BDM_STATUS_READY) {
            last_status_word = data_in;
            return true;
        }
        if (bdm_timeout_exceeded())
            break;
    }

    return false;
}

bool bdm_enable(void)
{
    __disable_irq();

    dsclk_low();
    dsi_high();

    target_reset_low();

    delay_us(10);

    target_reset_high();

    bdm_timeout_start();

    for (uint16_t i = 0; i < 1000; i++) {
        if (!freeze_read()) {
            __enable_irq();
            in_bdm_mode = true;
            return true;
        }
        if (bdm_timeout_exceeded())
            break;
        delay_us(10);
    }

    __enable_irq();
    in_bdm_mode = false;
    return false;
}

static bool bdm_send_preamble(void)
{
    __disable_irq();

    dsclk_low();
    dsi_high();

    bdm_delay_full_period();

    bdm_timeout_start();

    for (uint16_t i = 0; i < 1000; i++) {
        if (!freeze_read()) {
            __enable_irq();
            return true;
        }
        if (bdm_timeout_exceeded())
            break;
        delay_us(10);
    }

    __enable_irq();
    return false;
}

static bool bdm_check_status(void)
{
    if (!bdm_poll_ready())
        return false;
    return !bdm_status_is_error(last_status_word);
}

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

    bdm_shift_word(opcode, false);
    bdm_shift_word((uint16_t)(addr >> 16), false);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), false);

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

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

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

    bdm_shift_word(opcode, false);
    bdm_shift_word((uint16_t)(addr >> 16), false);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), false);

    if (size == BDM_SIZE_LONG) {
        bdm_shift_word((uint16_t)(data >> 16), false);
        bdm_shift_word((uint16_t)(data & 0xFFFFU), false);
    } else if (size == BDM_SIZE_WORD) {
        bdm_shift_word((uint16_t)data, false);
    } else {
        bdm_shift_word((uint16_t)data, false);
    }

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

bdm_result_t bdm_dump_memory(uint32_t addr, uint8_t size, uint8_t count, uint32_t *data)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint32_t dummy;
    bdm_result_t res = bdm_read_memory(addr, size, &dummy);
    if (res != BDM_OK)
        return res;

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

bdm_result_t bdm_fill_memory(uint32_t addr, uint8_t size, uint32_t data, uint8_t count)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_result_t res = bdm_write_memory(addr, size, data);
    if (res != BDM_OK)
        return res;

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

bdm_result_t bdm_read_data_reg(uint8_t reg, uint32_t *value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RAREG | BDM_REG_CLASS_DATA | (reg & 0x07);
    bdm_shift_word(opcode, false);

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

bdm_result_t bdm_write_data_reg(uint8_t reg, uint32_t value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WAREG | BDM_REG_CLASS_DATA | (reg & 0x07);
    bdm_shift_word(opcode, false);

    bdm_shift_word((uint16_t)(value >> 16), false);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

bdm_result_t bdm_read_addr_reg(uint8_t reg, uint32_t *value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RAREG | BDM_REG_CLASS_ADDR | (reg & 0x07);
    bdm_shift_word(opcode, false);

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

bdm_result_t bdm_write_addr_reg(uint8_t reg, uint32_t value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WAREG | BDM_REG_CLASS_ADDR | (reg & 0x07);
    bdm_shift_word(opcode, false);

    bdm_shift_word((uint16_t)(value >> 16), false);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

bdm_result_t bdm_read_sysreg(uint8_t select, uint32_t *value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_RSREG | (select << 3);
    bdm_shift_word(opcode, false);

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

bdm_result_t bdm_write_sysreg(uint8_t select, uint32_t value)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint16_t opcode = BDM_OPCODE_WSREG | (select << 3);
    bdm_shift_word(opcode, false);

    bdm_shift_word((uint16_t)(value >> 16), false);
    bdm_shift_word((uint16_t)(value & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

bdm_result_t bdm_target_reset(void)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_RST, false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;

    target_reset_low();
    delay_us(10);
    target_reset_high();

    return BDM_OK;
}

bdm_result_t bdm_target_halt(void)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    uint32_t atemp;
    bdm_read_sysreg(BDM_SR_ATEMP, &atemp);

    return BDM_OK;
}

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

bdm_result_t bdm_call(uint32_t addr)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_CALL, false);

    bdm_shift_word((uint16_t)(addr >> 16), false);
    bdm_shift_word((uint16_t)(addr & 0xFFFFU), false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;

    in_bdm_mode = false;
    return BDM_OK;
}

bdm_result_t bdm_step(void)
{
    uint32_t pc;

    bdm_result_t res = bdm_read_sysreg(BDM_SR_PCC, &pc);
    if (res != BDM_OK)
        return res;

    uint32_t instr;
    res = bdm_read_memory(pc, BDM_SIZE_WORD, &instr);
    if (res != BDM_OK)
        return res;

    uint16_t instr_len = 2;

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

    uint32_t next_pc = pc + instr_len;
    bdm_write_sysreg(BDM_SR_RPC, next_pc);

    return BDM_OK;
}

bdm_result_t bdm_nop(void)
{
    if (!bdm_send_preamble())
        return BDM_ERR_NO_TARGET;

    bdm_shift_word(BDM_OPCODE_NOP, false);

    if (!bdm_check_status())
        return BDM_ERR_BERR;
    return BDM_OK;
}

bool bdm_in_bdm_mode(void)
{
    return in_bdm_mode;
}

void bdm_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;

    bdm_gpio_set_output(BDM_DSCLK_GPIO, BDM_DSCLK_PIN);
    bdm_gpio_set_output(BDM_DSI_GPIO, BDM_DSI_PIN);
    bdm_gpio_set_output(BDM_RESET_GPIO, BDM_RESET_PIN);

    bdm_gpio_set(BDM_DSCLK_GPIO, BDM_DSCLK_PIN);
    bdm_gpio_clr(BDM_DSI_GPIO, BDM_DSI_PIN);
    bdm_gpio_set(BDM_RESET_GPIO, BDM_RESET_PIN);

    bdm_gpio_set_input_pullup(BDM_FREEZE_GPIO, BDM_FREEZE_PIN);
    bdm_gpio_set_input_pullup(BDM_DSO_GPIO, BDM_DSO_PIN);

    in_bdm_mode = false;
    bdm_timing_init();
}
