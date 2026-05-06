#ifndef BDM_CORE_H
#define BDM_CORE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BDM_STATE_IDLE,
    BDM_STATE_PREAMBLE,
    BDM_STATE_COMMAND,
    BDM_STATE_ADDRESS,
    BDM_STATE_DATA,
    BDM_STATE_CHECKSUM
} bdm_state_t;

typedef enum {
    BDM_CMD_NOP        = 0x00,
    BDM_CMD_READ_MEM   = 0x01,
    BDM_CMD_WRITE_MEM  = 0x02,
    BDM_CMD_READ_REG   = 0x10,
    BDM_CMD_WRITE_REG  = 0x11,
    BDM_CMD_TARGET_RST = 0x20,
    BDM_CMD_TARGET_HALT= 0x21,
    BDM_CMD_TARGET_GO  = 0x22,
    BDM_CMD_STEP       = 0x23
} bdm_cmd_t;

void     bdm_init(void);
bool     bdm_send_preamble(void);
bool     bdm_shift_byte(uint8_t out, uint8_t *in);
bool     bdm_read_memory(uint32_t addr, uint8_t *data, uint8_t size);
bool     bdm_write_memory(uint32_t addr, const uint8_t *data, uint8_t size);
bool     bdm_read_register(uint8_t reg, uint32_t *value);
bool     bdm_write_register(uint8_t reg, uint32_t value);
bool     bdm_target_reset(void);
bool     bdm_target_halt(void);
bool     bdm_target_go(void);
bool     bdm_step(void);

#endif
