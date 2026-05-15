#include "board_config.h"
#include "hal.h"
#include "sim_bdm.h"

void sim_bdm_init(void)
{
    /* DSCLK (BKPT): input with pull-up (debugger drives) */
    hal_gpio_set_input_pullup(DSCLK_PORT, DSCLK_PIN);

    /* DSI (IFETCH): input with pull-up (debugger drives) */
    hal_gpio_set_input_pullup(DSI_PORT, DSI_PIN);

    /* DSO (IPIPE): output, default high (NOT READY state) */
    hal_gpio_set_output(DSO_PORT, DSO_PIN);
    hal_gpio_set_high(DSO_PORT, DSO_PIN);

    /* FREEZE: output, default high (not in BDM mode) */
    hal_gpio_set_output(FREEZE_PORT, FREEZE_PIN);
    hal_gpio_set_high(FREEZE_PORT, FREEZE_PIN);

    /* TARGET_RESET: input with pull-up (debugger drives) */
    hal_gpio_set_input_pullup(TARGET_RESET_PORT, TARGET_RESET_PIN);
}

uint16_t sim_bdm_shift_word(uint16_t out_data, bool out_ready)
{
    uint16_t data_in = 0;

    /* Per spec: data transitions on falling edge of DSCLK,
       stable by rising edge, latched on rising edge. */

    for (uint8_t bit = 0; bit < 17; bit++) {
        /* Determine DSO value for this bit */
        uint8_t dso_bit;
        if (bit < 16) {
            dso_bit = (out_data >> (15 - bit)) & 1;
        } else {
            /* Bit 16: status bit (0=READY, 1=NOT READY) */
            dso_bit = out_ready ? 0 : 1;
        }

        /* Drive DSO on falling edge (data transition phase) */
        if (dso_bit)
            hal_gpio_set_high(DSO_PORT, DSO_PIN);
        else
            hal_gpio_set_low(DSO_PORT, DSO_PIN);

        /* Wait for DSCLK falling edge if currently high */
        while (hal_gpio_read(DSCLK_PORT, DSCLK_PIN))
            ;

        /* Wait for DSCLK rising edge (sample phase) */
        while (!hal_gpio_read(DSCLK_PORT, DSCLK_PIN))
            ;

        /* Sample DSI on rising edge */
        if (bit < 16) {
            if (hal_gpio_read(DSI_PORT, DSI_PIN))
                data_in |= (uint16_t)(1 << (15 - bit));
        }
    }

    /* Per spec: DSO returns to high (NOT READY) between transfers */
    hal_gpio_set_high(DSO_PORT, DSO_PIN);

    return data_in;
}

bool sim_bdm_wait_preamble(void)
{
    /* Wait for DSCLK (BKPT) to go low */
    uint32_t timeout = 100000;
    while (hal_gpio_read(DSCLK_PORT, DSCLK_PIN)) {
        if (--timeout == 0)
            return false;
        hal_delay_us(10);
    }
    return true;
}

void sim_bdm_assert_freeze(void)
{
    hal_gpio_set_low(FREEZE_PORT, FREEZE_PIN);
}

void sim_bdm_deassert_freeze(void)
{
    hal_gpio_set_high(FREEZE_PORT, FREEZE_PIN);
}

bool sim_bdm_reset_asserted(void)
{
    return hal_gpio_read(TARGET_RESET_PORT, TARGET_RESET_PIN) == 0;
}

bool sim_bdm_bkpt_sampled(void)
{
    return hal_gpio_read(DSCLK_PORT, DSCLK_PIN) == 0;
}
