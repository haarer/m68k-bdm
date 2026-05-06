#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "config.h"
#include "uart.h"
#include "protocol.h"
#include "bdm_core.h"

static void handle_command(protocol_command_t *cmd)
{
    protocol_response_t rsp;

    switch (cmd->cmd) {
    case CMD_MEM_READ:
        if (cmd->len >= 4) {
            uint32_t addr = ((uint32_t)cmd->payload[0] << 24) |
                            ((uint32_t)cmd->payload[1] << 16) |
                            ((uint32_t)cmd->payload[2] << 8)  |
                            (uint32_t)cmd->payload[3];
            uint8_t  count = (cmd->len > 4) ? cmd->payload[4] : 1;

            uint8_t data[256];
            if (bdm_read_memory(addr, data, count)) {
                rsp.code = RSP_OK;
                rsp.len  = count;
                memcpy(rsp.payload, data, count);
            } else {
                protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
                return;
            }
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    case CMD_MEM_WRITE:
        if (cmd->len >= 5) {
            uint32_t addr = ((uint32_t)cmd->payload[0] << 24) |
                            ((uint32_t)cmd->payload[1] << 16) |
                            ((uint32_t)cmd->payload[2] << 8)  |
                            (uint32_t)cmd->payload[3];

            if (bdm_write_memory(addr, &cmd->payload[4], cmd->len - 4)) {
                rsp.code = RSP_OK;
                rsp.len  = 0;
            } else {
                protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
                return;
            }
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    case CMD_REG_READ:
        if (cmd->len >= 1) {
            uint32_t value = 0;
            if (bdm_read_register(cmd->payload[0], &value)) {
                rsp.code = RSP_OK;
                rsp.len  = 4;
                rsp.payload[0] = (value >> 24) & 0xFF;
                rsp.payload[1] = (value >> 16) & 0xFF;
                rsp.payload[2] = (value >> 8)  & 0xFF;
                rsp.payload[3] = value & 0xFF;
            } else {
                protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
                return;
            }
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    case CMD_REG_WRITE:
        if (cmd->len >= 5) {
            uint32_t value = ((uint32_t)cmd->payload[1] << 24) |
                             ((uint32_t)cmd->payload[2] << 16) |
                             ((uint32_t)cmd->payload[3] << 8)  |
                             (uint32_t)cmd->payload[4];

            if (bdm_write_register(cmd->payload[0], value)) {
                rsp.code = RSP_OK;
                rsp.len  = 0;
            } else {
                protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
                return;
            }
        } else {
            protocol_send_error(cmd->cmd, RSP_ERROR);
            return;
        }
        break;

    case CMD_TARGET_RESET:
        if (bdm_target_reset()) {
            rsp.code = RSP_OK;
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
            return;
        }
        break;

    case CMD_TARGET_HALT:
        if (bdm_target_halt()) {
            rsp.code = RSP_OK;
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
            return;
        }
        break;

    case CMD_TARGET_GO:
        if (bdm_target_go()) {
            rsp.code = RSP_OK;
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
            return;
        }
        break;

    case CMD_STEP:
        if (bdm_step()) {
            rsp.code = RSP_OK;
            rsp.len  = 0;
        } else {
            protocol_send_error(cmd->cmd, RSP_TARGET_ERROR);
            return;
        }
        break;

    case CMD_STATUS:
        rsp.code = RSP_OK;
        rsp.len  = 1;
        rsp.payload[0] = 0x01;
        break;

    case CMD_CONFIG:
        rsp.code = RSP_OK;
        rsp.len  = 0;
        break;

    default:
        protocol_send_error(cmd->cmd, RSP_NOT_SUPPORTED);
        return;
    }

    if (cmd->cmd != 0xFF)
        protocol_send_response(&rsp);
}

int main(void)
{
    uart_init(SERIAL_UBRR_VALUE);
    bdm_init();

    sei();

    protocol_command_t cmd;

    while (1) {
        uint8_t byte;
        if (uart_receive(&byte)) {
            if (protocol_parse_command(byte, &cmd)) {
                cli();
                handle_command(&cmd);
                sei();
            }
        }
    }

    return 0;
}
