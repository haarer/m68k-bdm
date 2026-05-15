#include <string.h>
#include "config.h"
#include "hal.h"
#include "protocol.h"
#include "checksum.h"

#define PARSE_STATE_WAIT_STX  0
#define PARSE_STATE_CMD       1
#define PARSE_STATE_LEN       2
#define PARSE_STATE_PAYLOAD   3
#define PARSE_STATE_CS        4
#define PARSE_STATE_ETX       5

static uint8_t parse_state = PARSE_STATE_WAIT_STX;
static protocol_command_t pending_cmd;
static uint8_t payload_idx;
static uint8_t cs_expected;

bool protocol_parse_command(uint8_t byte, protocol_command_t *cmd)
{
    switch (parse_state) {
    case PARSE_STATE_WAIT_STX:
        if (byte == PROTOCOL_STX) {
            parse_state = PARSE_STATE_CMD;
        }
        break;

    case PARSE_STATE_CMD:
        pending_cmd.cmd = byte;
        parse_state = PARSE_STATE_LEN;
        break;

    case PARSE_STATE_LEN:
        pending_cmd.len = byte;
        payload_idx = 0;
        cs_expected = 0;
        cs_expected ^= PROTOCOL_STX;
        cs_expected ^= pending_cmd.cmd;
        cs_expected ^= pending_cmd.len;
        parse_state = (byte > 0) ? PARSE_STATE_PAYLOAD : PARSE_STATE_CS;
        break;

    case PARSE_STATE_PAYLOAD:
        pending_cmd.payload[payload_idx++] = byte;
        cs_expected ^= byte;
        if (payload_idx >= pending_cmd.len) {
            parse_state = PARSE_STATE_CS;
        }
        break;

    case PARSE_STATE_CS:
        if (byte != cs_expected) {
            parse_state = PARSE_STATE_WAIT_STX;
            if (byte == PROTOCOL_STX) {
                parse_state = PARSE_STATE_CMD;
            }
            return false;
        }
        parse_state = PARSE_STATE_ETX;
        break;

    case PARSE_STATE_ETX:
        if (byte == PROTOCOL_ETX) {
            parse_state = PARSE_STATE_WAIT_STX;
            memcpy(cmd, &pending_cmd, sizeof(protocol_command_t));
            return true;
        }
        parse_state = PARSE_STATE_WAIT_STX;
        if (byte == PROTOCOL_STX) {
            parse_state = PARSE_STATE_CMD;
        }
        return false;

    default:
        parse_state = PARSE_STATE_WAIT_STX;
        break;
    }
    return false;
}

void protocol_send_response(protocol_response_t *rsp)
{
    uint8_t cs = 0;

    hal_serial_putc(PROTOCOL_STX);
    cs ^= PROTOCOL_STX;

    hal_serial_putc(0x80 | rsp->code);
    cs ^= (0x80 | rsp->code);

    hal_serial_putc(rsp->len);
    cs ^= rsp->len;

    for (uint8_t i = 0; i < rsp->len; i++) {
        hal_serial_putc(rsp->payload[i]);
        cs ^= rsp->payload[i];
    }

    hal_serial_putc(cs);
    hal_serial_putc(PROTOCOL_ETX);
    hal_serial_flush();
}

void protocol_send_error(uint8_t cmd, uint8_t error_code)
{
    protocol_response_t rsp;
    rsp.code = error_code;
    rsp.len  = 1;
    rsp.payload[0] = cmd;
    protocol_send_response(&rsp);
}
