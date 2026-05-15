#include <string.h>
#include "config.h"
#include "hal.h"
#include "serial/protocol.h"
#include "bdm_engine/bdm_core.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static inline uint32_t payload_to_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           (uint32_t)p[3];
}

static inline void u32_to_payload(uint32_t v, uint8_t *p)
{
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >>  8) & 0xFF);
    p[3] = (uint8_t)( v        & 0xFF);
}

static inline bdm_size_t size_from_byte(uint8_t s)
{
    switch (s) {
    case 2: return BDM_SIZE_WORD;
    case 4: return BDM_SIZE_LONG;
    case 1:
    default: return BDM_SIZE_BYTE;
    }
}

static inline bdm_result_t bdm_result_to_rsp(bdm_result_t r)
{
    switch (r) {
    case BDM_OK:          return RSP_OK;
    case BDM_ERR_TIMEOUT: return RSP_TIMEOUT;
    case BDM_ERR_BERR:    return RSP_TARGET_ERROR;
    case BDM_ERR_NO_TARGET: return RSP_TARGET_ERROR;
    case BDM_ERR_NOT_READY: return RSP_ERROR;
    case BDM_ERR_ILLEGAL:   return RSP_ERROR;
    default:              return RSP_ERROR;
    }
}

/* ------------------------------------------------------------------ */
/*  Command dispatcher                                                 */
/* ------------------------------------------------------------------ */

static void handle_command(protocol_command_t *cmd)
{
    protocol_response_t rsp;

    switch (cmd->cmd) {

    /* ---- Memory read ---- */
    case CMD_MEM_READ:
        if (cmd->len >= 5) {
            uint32_t addr = payload_to_u32(cmd->payload);
            uint8_t  count = cmd->payload[4];
            bdm_size_t sz  = (cmd->len > 5) ? size_from_byte(cmd->payload[5]) : BDM_SIZE_BYTE;

            uint8_t data_buf[256];
            uint32_t val;

            for (uint8_t i = 0; i < count && i < 64; i++) {
                bdm_result_t res = bdm_read_memory(addr, sz, &val);
                if (res != BDM_OK) {
                    rsp.code = bdm_result_to_rsp(res);
                    rsp.len  = 0;
                    protocol_send_response(&rsp);
                    return;
                }
                if (sz == BDM_SIZE_BYTE)
                    data_buf[i] = (uint8_t)(val & 0xFF);
                else
                    u32_to_payload(val, &data_buf[i * 4]);
            }

            rsp.code = RSP_OK;
            rsp.len  = count;
            memcpy(rsp.payload, data_buf, count);
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- Memory write ---- */
    case CMD_MEM_WRITE:
        if (cmd->len >= 5) {
            uint32_t addr = payload_to_u32(cmd->payload);
            uint8_t  data_start = 4;

            for (uint8_t i = data_start; i < cmd->len; i++) {
                uint32_t val = (uint32_t)cmd->payload[i];
                bdm_result_t res = bdm_write_memory(addr, BDM_SIZE_BYTE, val);
                if (res != BDM_OK) {
                    rsp.code = bdm_result_to_rsp(res);
                    rsp.len  = 0;
                    protocol_send_response(&rsp);
                    return;
                }
            }

            rsp.code = RSP_OK;
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- Data register read ---- */
    case CMD_REG_READ:
        if (cmd->len >= 1) {
            uint32_t value = 0;
            uint8_t reg = cmd->payload[0];
            bdm_result_t res;

            if (reg < 8)
                res = bdm_read_data_reg(reg, &value);
            else
                res = bdm_read_addr_reg(reg - 8, &value);

            if (res == BDM_OK) {
                rsp.code = RSP_OK;
                rsp.len  = 4;
                u32_to_payload(value, rsp.payload);
            } else {
                rsp.code = bdm_result_to_rsp(res);
                rsp.len  = 0;
            }
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- Data register write ---- */
    case CMD_REG_WRITE:
        if (cmd->len >= 5) {
            uint8_t reg = cmd->payload[0];
            uint32_t value = payload_to_u32(&cmd->payload[1]);
            bdm_result_t res;

            if (reg < 8)
                res = bdm_write_data_reg(reg, value);
            else
                res = bdm_write_addr_reg(reg - 8, value);

            rsp.code = bdm_result_to_rsp(res);
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- System register read ---- */
    case CMD_SYSREG_READ:
        if (cmd->len >= 1) {
            uint32_t value = 0;
            bdm_result_t res = bdm_read_sysreg(cmd->payload[0], &value);

            if (res == BDM_OK) {
                rsp.code = RSP_OK;
                rsp.len  = 4;
                u32_to_payload(value, rsp.payload);
            } else {
                rsp.code = bdm_result_to_rsp(res);
                rsp.len  = 0;
            }
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- System register write ---- */
    case CMD_SYSREG_WRITE:
        if (cmd->len >= 5) {
            uint8_t reg = cmd->payload[0];
            uint32_t value = payload_to_u32(&cmd->payload[1]);
            bdm_result_t res = bdm_write_sysreg(reg, value);
            rsp.code = bdm_result_to_rsp(res);
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- Memory dump (bulk read) ---- */
    case CMD_MEM_DUMP:
        if (cmd->len >= 5) {
            uint32_t addr  = payload_to_u32(cmd->payload);
            uint8_t  count = cmd->payload[4];
            bdm_size_t sz  = (cmd->len > 5) ? size_from_byte(cmd->payload[5]) : BDM_SIZE_BYTE;

            uint32_t vals[64];
            bdm_result_t res = bdm_dump_memory(addr, sz, count, vals);

            if (res == BDM_OK) {
                rsp.code = RSP_OK;
                rsp.len  = count * 4;
                for (uint8_t i = 0; i < count; i++)
                    u32_to_payload(vals[i], &rsp.payload[i * 4]);
            } else {
                rsp.code = bdm_result_to_rsp(res);
                rsp.len  = 0;
            }
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- Memory fill (bulk write) ---- */
    case CMD_MEM_FILL:
        if (cmd->len >= 10) {
            uint32_t addr  = payload_to_u32(cmd->payload);
            uint8_t  count = cmd->payload[4];
            uint32_t value = payload_to_u32(&cmd->payload[5]);
            bdm_size_t sz  = (cmd->len > 9) ? size_from_byte(cmd->payload[9]) : BDM_SIZE_BYTE;

            bdm_result_t res = bdm_fill_memory(addr, sz, value, count);
            rsp.code = bdm_result_to_rsp(res);
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- BDM Enable (one-time reset sequence, §7.2.1) ---- */
    case CMD_BDM_ENABLE:
    {
        bool ok = bdm_enable();
        rsp.code = ok ? RSP_OK : RSP_TARGET_ERROR;
        rsp.len  = 1;
        rsp.payload[0] = ok ? 0x01 : 0x00;
    }
    break;

    /* ---- Target reset ---- */
    case CMD_TARGET_RESET:
    {
        bdm_result_t res = bdm_target_reset();
        rsp.code = bdm_result_to_rsp(res);
        rsp.len  = 0;
    }
    break;

    /* ---- Target halt ---- */
    case CMD_TARGET_HALT:
    {
        bdm_result_t res = bdm_target_halt();
        rsp.code = bdm_result_to_rsp(res);
        rsp.len  = 0;
    }
    break;

    /* ---- Target go ---- */
    case CMD_TARGET_GO:
    {
        bdm_result_t res = bdm_target_go();
        rsp.code = bdm_result_to_rsp(res);
        rsp.len  = 0;
    }
    break;

    /* ---- Step ---- */
    case CMD_STEP:
    {
        bdm_result_t res = bdm_step();
        rsp.code = bdm_result_to_rsp(res);
        rsp.len  = 0;
    }
    break;

    /* ---- Call ---- */
    case CMD_CALL:
        if (cmd->len >= 4) {
            uint32_t addr = payload_to_u32(cmd->payload);
            bdm_result_t res = bdm_call(addr);
            rsp.code = bdm_result_to_rsp(res);
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    /* ---- Breakpoint set ---- */
    case CMD_BREAKPOINT_SET:
        rsp.code = RSP_OK;
        rsp.len  = 0;
        break;

    /* ---- Breakpoint clear ---- */
    case CMD_BREAKPOINT_CLR:
        rsp.code = RSP_OK;
        rsp.len  = 0;
        break;

    /* ---- Status ---- */
    case CMD_STATUS:
        rsp.code = RSP_OK;
        rsp.len  = 1;
        rsp.payload[0] = bdm_in_bdm_mode() ? 0x01 : 0x00;
        break;

    /* ---- Config ---- */
    case CMD_CONFIG:
        rsp.code = RSP_OK;
        rsp.len  = 0;
        break;

    /* ---- Unknown ---- */
    default:
        protocol_send_error(cmd->cmd, RSP_NOT_SUPPORTED);
        return;
    }

    protocol_send_response(&rsp);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    hal_serial_init(SERIAL_BAUD);
    hal_timer_init();
    bdm_init();

    hal_irq_enable();

    protocol_command_t cmd;

    while (1) {
        if (hal_serial_has_data()) {
            uint8_t byte = hal_serial_getc();
            if (protocol_parse_command(byte, &cmd)) {
                hal_irq_disable();
                handle_command(&cmd);
                hal_irq_enable();
            }
        }
    }

    return 0;
}
