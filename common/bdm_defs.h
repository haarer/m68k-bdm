#ifndef BDM_DEFS_H
#define BDM_DEFS_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  BDM Opcodes (CPU32 Reference Manual §7.2.8)                        */
/* ------------------------------------------------------------------ */

#define BDM_OPCODE_NOP        0x0000U
#define BDM_OPCODE_RST        0x0100U
#define BDM_OPCODE_CALL       0x0200U
#define BDM_OPCODE_GO         0x0300U
#define BDM_OPCODE_WAREG      0x4100U
#define BDM_OPCODE_RAREG      0x4200U
#define BDM_OPCODE_WSREG      0x2400U
#define BDM_OPCODE_RSREG      0x2500U
#define BDM_OPCODE_WRITE      0x0C00U
#define BDM_OPCODE_READ       0x0B00U
#define BDM_OPCODE_FILL       0x0E00U
#define BDM_OPCODE_DUMP       0x0F00U

/* ------------------------------------------------------------------ */
/*  Operand Size Encoding (bits [3:2] in opcode)                       */
/* ------------------------------------------------------------------ */

#define BDM_SIZE_BYTE      0x0000U
#define BDM_SIZE_WORD      0x0008U
#define BDM_SIZE_LONG      0x0010U

/* ------------------------------------------------------------------ */
/*  Register Class (for RAREG/WAREG)                                   */
/* ------------------------------------------------------------------ */

#define BDM_REG_CLASS_DATA    0x0000U
#define BDM_REG_CLASS_ADDR    0x0004U

/* ------------------------------------------------------------------ */
/*  System Register Select Codes (RSREG/WSREG, bits [6:3])            */
/* ------------------------------------------------------------------ */

#define BDM_SR_RPC            0x0000U
#define BDM_SR_PCC            0x0008U
#define BDM_SR_ATEMP          0x0010U
#define BDM_SR_FAR            0x0018U
#define BDM_SR_VBR            0x0020U
#define BDM_SR_SR             0x0028U
#define BDM_SR_USP            0x0030U
#define BDM_SR_SSP            0x0038U
#define BDM_SR_SFC            0x0040U
#define BDM_SR_DFC            0x0048U

/* ------------------------------------------------------------------ */
/*  BDM Status (DSO bit 16)                                            */
/* ------------------------------------------------------------------ */

#define BDM_STATUS_READY      0
#define BDM_STATUS_NOT_READY  1

/* ------------------------------------------------------------------ */
/*  16-bit Status Word Values (returned in data field)                 */
/* ------------------------------------------------------------------ */

#define BDM_STATUS_OK         0xFFFFU
#define BDM_STATUS_BERR       0x8001U
#define BDM_STATUS_ILLEGAL    0x0001U

/* ------------------------------------------------------------------ */
/*  Host Protocol Delimiters                                           */
/* ------------------------------------------------------------------ */

#define PROTOCOL_STX    0x02
#define PROTOCOL_ETX    0x03
#define PROTOCOL_MAX_PAYLOAD  256

/* ------------------------------------------------------------------ */
/*  Host Command Codes                                                 */
/* ------------------------------------------------------------------ */

#define CMD_BDM_ENABLE     0x10
#define CMD_MEM_READ       0x11
#define CMD_MEM_WRITE      0x12
#define CMD_REG_READ       0x13
#define CMD_REG_WRITE      0x14
#define CMD_TARGET_RESET   0x15
#define CMD_TARGET_HALT    0x16
#define CMD_TARGET_GO      0x17
#define CMD_STEP           0x18
#define CMD_BREAKPOINT_SET 0x19
#define CMD_BREAKPOINT_CLR 0x1A
#define CMD_STATUS         0x1B
#define CMD_CONFIG         0x1C
#define CMD_SYSREG_READ    0x1D
#define CMD_SYSREG_WRITE   0x1E
#define CMD_MEM_DUMP       0x1F
#define CMD_MEM_FILL       0x20
#define CMD_CALL           0x21

/* ------------------------------------------------------------------ */
/*  Host Response Codes                                                */
/* ------------------------------------------------------------------ */

#define RSP_OK            0x00
#define RSP_ERROR         0x01
#define RSP_NOT_SUPPORTED 0x02
#define RSP_TIMEOUT       0x03
#define RSP_TARGET_ERROR  0x04

#endif
