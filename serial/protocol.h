#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t  cmd;
    uint8_t  len;
    uint8_t  payload[256];
} protocol_command_t;

typedef struct {
    uint8_t  code;
    uint8_t  len;
    uint8_t  payload[256];
} protocol_response_t;

bool     protocol_parse_command(uint8_t byte, protocol_command_t *cmd);
void     protocol_send_response(protocol_response_t *rsp);
void     protocol_send_error(uint8_t cmd, uint8_t error_code);

#endif
