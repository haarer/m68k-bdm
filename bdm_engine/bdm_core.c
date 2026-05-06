#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "config.h"
#include "bdm_timing.h"
#include "bdm_core.h"

static void bdm_set_clock(bool high)
{
    if (high)
        BDMC_PORT |=  (1 << BDMC_BIT);
    else
        BDMC_PORT &= ~(1 << BDMC_BIT);
}

static void bdm_set_data(bool high)
{
    if (high)
        BDD_PORT |=  (1 << BDD_BIT);
    else
        BDD_PORT &= ~(1 << BDD_BIT);
}

static bool bdm_read_data(void)
{
    return (BDD_PIN & (1 << BDD_BIT)) != 0;
}

static void bdm_set_bdd_output(void)
{
    BDD_DDR |= (1 << BDD_BIT);
}

static void bdm_set_bdd_input(void)
{
    BDD_DDR &= ~(1 << BDD_BIT);
}

bool bdm_send_preamble(void)
{
    cli();

    bdm_set_bdd_output();
    bdm_set_data(true);
    bdm_set_clock(false);

    bdm_delay_full_period();

    for (uint8_t i = 0; i < 8; i++) {
        bdm_set_clock(true);
        bdm_delay_half_period();
        bdm_set_clock(false);
        bdm_delay_half_period();
    }

    bdm_set_bdd_input();

    sei();
    return true;
}

bool bdm_shift_byte(uint8_t out, uint8_t *in)
{
    cli();

    uint8_t result = 0;

    for (uint8_t bit = 0; bit < 8; bit++) {
        bdm_set_clock(false);
        bdm_delay_half_period();

        if (bit < 7) {
            bdm_set_bdd_output();
            if (out & (1 << (7 - bit)))
                bdm_set_data(true);
            else
                bdm_set_data(false);
        } else {
            bdm_set_bdd_input();
        }

        bdm_set_clock(true);
        bdm_delay_half_period();

        if (bit == 7) {
            result = bdm_read_data() ? 1 : 0;
        }
    }

    bdm_set_bdd_output();

    sei();

    if (in)
        *in = result;

    return true;
}

bool bdm_read_memory(uint32_t addr, uint8_t *data, uint8_t size)
{
    (void)addr;
    (void)data;
    (void)size;

    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_READ_MEM, NULL);

    for (uint8_t i = 0; i < size; i++) {
        uint8_t dummy = 0;
        bdm_shift_byte(0x00, &dummy);
        data[i] = dummy;
    }

    return true;
}

bool bdm_write_memory(uint32_t addr, const uint8_t *data, uint8_t size)
{
    (void)addr;
    (void)data;
    (void)size;

    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_WRITE_MEM, NULL);

    for (uint8_t i = 0; i < size; i++) {
        bdm_shift_byte(data[i], NULL);
    }

    return true;
}

bool bdm_read_register(uint8_t reg, uint32_t *value)
{
    (void)reg;
    (void)value;

    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_READ_REG, NULL);

    return true;
}

bool bdm_write_register(uint8_t reg, uint32_t value)
{
    (void)reg;
    (void)value;

    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_WRITE_REG, NULL);

    return true;
}

bool bdm_target_reset(void)
{
    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_TARGET_RST, NULL);

    TARGET_RESET_PORT &= ~(1 << TARGET_RESET_BIT);
    bdm_delay_us(10);
    TARGET_RESET_PORT |= (1 << TARGET_RESET_BIT);

    return true;
}

bool bdm_target_halt(void)
{
    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_TARGET_HALT, NULL);

    return true;
}

bool bdm_target_go(void)
{
    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_TARGET_GO, NULL);

    return true;
}

bool bdm_step(void)
{
    bdm_send_preamble();

    bdm_shift_byte(BDM_CMD_STEP, NULL);

    return true;
}

void bdm_init(void)
{
    BDMC_DDR  |=  (1 << BDMC_BIT);
    BDD_DDR   |=  (1 << BDD_BIT);
    BDREQ_DDR &= ~(1 << BDREQ_BIT);
    BDMACK_DDR&= ~(1 << BDMACK_BIT);
    TARGET_RESET_DDR |= (1 << TARGET_RESET_BIT);

    BDMC_PORT  &= ~(1 << BDMC_BIT);
    BDD_PORT   &= ~(1 << BDD_BIT);
    TARGET_RESET_PORT |= (1 << TARGET_RESET_BIT);

    bdm_timing_init();
}
