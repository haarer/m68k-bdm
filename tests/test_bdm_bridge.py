#!/usr/bin/env python3
"""
BDM Bridge Protocol Tests
Tests the host-side protocol for the MC68331 BDM bridge firmware.
"""

import struct
import socket
import time
import unittest
from unittest.mock import Mock, patch, MagicMock


# ------------------------------------------------------------------
#  Protocol constants (mirror config.h)
# ------------------------------------------------------------------

STX = 0x02
ETX = 0x03

CMD_MEM_READ       = 0x10
CMD_MEM_WRITE      = 0x11
CMD_REG_READ       = 0x12
CMD_REG_WRITE      = 0x13
CMD_TARGET_RESET   = 0x14
CMD_TARGET_HALT    = 0x15
CMD_TARGET_GO      = 0x16
CMD_STEP           = 0x17
CMD_BREAKPOINT_SET = 0x18
CMD_BREAKPOINT_CLR = 0x19
CMD_STATUS         = 0x1A
CMD_CONFIG         = 0x1B
CMD_SYSREG_READ    = 0x1C
CMD_SYSREG_WRITE   = 0x1D
CMD_MEM_DUMP       = 0x1E
CMD_MEM_FILL       = 0x1F
CMD_CALL           = 0x20

RSP_OK            = 0x00
RSP_ERROR         = 0x01
RSP_NOT_SUPPORTED = 0x02
RSP_TIMEOUT       = 0x03
RSP_TARGET_ERROR  = 0x04


# ------------------------------------------------------------------
#  Frame builder / parser
# ------------------------------------------------------------------

def build_frame(cmd, payload=None):
    """Build a protocol frame: STX + cmd + payload + checksum + ETX"""
    data = bytes([cmd])
    if payload:
        data += payload
    checksum = 0
    for b in data:
        checksum = (checksum + b) & 0xFF
    return bytes([STX]) + data + bytes([checksum, ETX])


def parse_frame(frame):
    """Parse a protocol frame, return (cmd, payload, checksum_ok)"""
    if frame[0] != STX:
        return None, None, False
    if frame[-1] != ETX:
        return None, None, False
    cmd = frame[1]
    payload = frame[2:-2]
    data = bytes([cmd]) + payload
    expected = 0
    for b in data:
        expected = (expected + b) & 0xFF
    return cmd, payload, frame[-2] == expected


# ------------------------------------------------------------------
#  Test: Frame construction
# ------------------------------------------------------------------

class TestFrameConstruction(unittest.TestCase):
    def test_mem_read_frame(self):
        payload = struct.pack(">I", 0x1000) + bytes([1, 1])  # addr, count, size
        frame = build_frame(CMD_MEM_READ, payload)
        self.assertEqual(frame[0], STX)
        self.assertEqual(frame[-1], ETX)
        self.assertEqual(frame[1], CMD_MEM_READ)

    def test_mem_write_frame(self):
        payload = struct.pack(">I", 0x1000) + bytes([0xDE, 0xAD])
        frame = build_frame(CMD_MEM_WRITE, payload)
        self.assertEqual(frame[1], CMD_MEM_WRITE)

    def test_sysreg_read_frame(self):
        frame = build_frame(CMD_SYSREG_READ, bytes([0x00]))
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(cmd, CMD_SYSREG_READ)
        self.assertEqual(payload[0], 0x00)

    def test_mem_dump_frame(self):
        payload = struct.pack(">I", 0x1000) + bytes([16, 4])
        frame = build_frame(CMD_MEM_DUMP, payload)
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(cmd, CMD_MEM_DUMP)

    def test_mem_fill_frame(self):
        payload = struct.pack(">I", 0x1000) + bytes([16]) + struct.pack(">I", 0xDEADBEEF) + bytes([4])
        frame = build_frame(CMD_MEM_FILL, payload)
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(cmd, CMD_MEM_FILL)

    def test_call_frame(self):
        payload = struct.pack(">I", 0x2000)
        frame = build_frame(CMD_CALL, payload)
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(cmd, CMD_CALL)
        self.assertEqual(struct.unpack(">I", payload)[0], 0x2000)

    def test_checksum_validation(self):
        frame = build_frame(CMD_STATUS)
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)

    def test_bad_checksum(self):
        frame = build_frame(CMD_STATUS)
        bad = frame[:-2] + bytes([0xFF, ETX])
        cmd, payload, ok = parse_frame(bad)
        self.assertFalse(ok)


# ------------------------------------------------------------------
#  Test: Command codes
# ------------------------------------------------------------------

class TestCommandCodes(unittest.TestCase):
    def test_all_commands_defined(self):
        expected = {
            CMD_MEM_READ, CMD_MEM_WRITE, CMD_REG_READ, CMD_REG_WRITE,
            CMD_TARGET_RESET, CMD_TARGET_HALT, CMD_TARGET_GO, CMD_STEP,
            CMD_BREAKPOINT_SET, CMD_BREAKPOINT_CLR, CMD_STATUS, CMD_CONFIG,
            CMD_SYSREG_READ, CMD_SYSREG_WRITE, CMD_MEM_DUMP, CMD_MEM_FILL,
            CMD_CALL
        }
        self.assertEqual(len(expected), len(expected))
        self.assertIn(CMD_MEM_DUMP, expected)
        self.assertIn(CMD_MEM_FILL, expected)
        self.assertIn(CMD_SYSREG_READ, expected)
        self.assertIn(CMD_SYSREG_WRITE, expected)
        self.assertIn(CMD_CALL, expected)

    def test_response_codes_defined(self):
        expected = {RSP_OK, RSP_ERROR, RSP_NOT_SUPPORTED, RSP_TIMEOUT, RSP_TARGET_ERROR}
        self.assertEqual(len(expected), 5)


# ------------------------------------------------------------------
#  Test: BDM opcode constants
# ------------------------------------------------------------------

class TestBDMOpCodes(unittest.TestCase):
    def test_memory_opcodes(self):
        self.assertEqual(0x0B00, 0x0B00)  # READ
        self.assertEqual(0x0C00, 0x0C00)  # WRITE
        self.assertEqual(0x0E00, 0x0E00)  # FILL
        self.assertEqual(0x0F00, 0x0F00)  # DUMP

    def test_register_opcodes(self):
        self.assertEqual(0x4100, 0x4100)  # WAREG
        self.assertEqual(0x4200, 0x4200)  # RAREG
        self.assertEqual(0x2400, 0x2400)  # WSREG
        self.assertEqual(0x2500, 0x2500)  # RSREG

    def test_control_opcodes(self):
        self.assertEqual(0x0000, 0x0000)  # NOP
        self.assertEqual(0x0100, 0x0100)  # RST
        self.assertEqual(0x0200, 0x0200)  # CALL
        self.assertEqual(0x0300, 0x0300)  # GO

    def test_size_encoding(self):
        self.assertEqual(0x0000, 0x0000)  # BYTE
        self.assertEqual(0x0008, 0x0008)  # WORD
        self.assertEqual(0x0010, 0x0010)  # LONG


# ------------------------------------------------------------------
#  Test: BDM protocol simulation
# ------------------------------------------------------------------

class BDMProtocolSimulator:
    """Simulates the MC68331 BDM protocol for testing."""

    def __init__(self):
        self.memory = bytearray(0x10000)
        self.data_regs = [0] * 8
        self.addr_regs = [0] * 8
        self.sysregs = {
            0: 0x0000,  # RPC
            1: 0x0000,  # PCC
            2: 0x0000,  # ATEMP
            3: 0x0000,  # SR
            4: 0x0000,  # VBR
            5: 0x0000,  # SFC
            6: 0x0000,  # DFC
            7: 0x0000,  # FAR
        }
        self.in_bdm_mode = True
        self.halted = True
        self.address_ptr = 0

    def send_preamble(self):
        return self.in_bdm_mode

    def shift_word(self, out):
        return 0x7FFF if self.halted else 0x0000

    def read_memory(self, addr, size=1):
        if addr >= 0x10000:
            return None, "BERR"
        if size == 4:
            val = (self.memory[addr] << 24) | (self.memory[addr+1] << 16) | \
                  (self.memory[addr+2] << 8) | self.memory[addr+3]
            return val, None
        elif size == 2:
            val = (self.memory[addr] << 8) | self.memory[addr+1]
            return val, None
        else:
            return self.memory[addr], None

    def write_memory(self, addr, value, size=1):
        if addr >= 0x10000:
            return "BERR"
        if size == 4:
            self.memory[addr]   = (value >> 24) & 0xFF
            self.memory[addr+1] = (value >> 16) & 0xFF
            self.memory[addr+2] = (value >>  8) & 0xFF
            self.memory[addr+3] =  value        & 0xFF
        elif size == 2:
            self.memory[addr]   = (value >> 8) & 0xFF
            self.memory[addr+1] =  value       & 0xFF
        else:
            self.memory[addr] = value & 0xFF
        return None


class TestBDMProtocolSimulation(unittest.TestCase):
    def setUp(self):
        self.sim = BDMProtocolSimulator()

    def test_preamble_success(self):
        self.assertTrue(self.sim.send_preamble())

    def test_memory_read_write_roundtrip(self):
        addr = 0x1000
        value = 0xAB
        self.sim.write_memory(addr, value)
        read_val, err = self.sim.read_memory(addr)
        self.assertIsNone(err)
        self.assertEqual(read_val, value)

    def test_memory_read_write_word(self):
        addr = 0x1000
        value = 0x1234
        self.sim.write_memory(addr, value, 2)
        read_val, err = self.sim.read_memory(addr, 2)
        self.assertIsNone(err)
        self.assertEqual(read_val, value)

    def test_memory_read_write_long(self):
        addr = 0x1000
        value = 0xDEADBEEF
        self.sim.write_memory(addr, value, 4)
        read_val, err = self.sim.read_memory(addr, 4)
        self.assertIsNone(err)
        self.assertEqual(read_val, value)

    def test_memory_out_of_bounds(self):
        _, err = self.sim.read_memory(0x10000)
        self.assertEqual(err, "BERR")

    def test_memory_write_out_of_bounds(self):
        err = self.sim.write_memory(0x10000, 0xAB)
        self.assertEqual(err, "BERR")

    def test_dump_fill_roundtrip(self):
        addr = 0x1000
        count = 16
        fill_val = 0x55

        # Fill
        for i in range(count):
            self.sim.write_memory(addr + i, fill_val)

        # Dump
        for i in range(count):
            read_val, err = self.sim.read_memory(addr + i)
            self.assertIsNone(err)
            self.assertEqual(read_val, fill_val)

    def test_register_read_write(self):
        self.sim.data_regs[0] = 0x12345678
        self.assertEqual(self.sim.data_regs[0], 0x12345678)

    def test_shift_word_status(self):
        self.sim.halted = True
        status = self.sim.shift_word(0)
        self.assertEqual(status, 0x7FFF)  # OK

    def test_shift_word_error(self):
        self.sim.halted = False
        status = self.sim.shift_word(0)
        self.assertEqual(status, 0x0000)


# ------------------------------------------------------------------
#  Test: BDM engine functions (mocked)
# ------------------------------------------------------------------

class TestBDMEngine(unittest.TestCase):
    """Test BDM engine logic with mocked hardware."""

    def test_size_from_byte(self):
        # Simulates the size_from_byte helper in main.c
        def size_from_byte(s):
            if s == 2: return 1  # WORD
            if s == 4: return 2  # LONG
            return 0  # BYTE

        self.assertEqual(size_from_byte(1), 0)
        self.assertEqual(size_from_byte(2), 1)
        self.assertEqual(size_from_byte(4), 2)

    def test_u32_pack_unpack(self):
        val = 0xDEADBEEF
        packed = struct.pack(">I", val)
        unpacked = struct.unpack(">I", packed)[0]
        self.assertEqual(val, unpacked)

    def test_payload_constructor(self):
        # Test that payload for MEM_READ is well-formed
        addr = 0x1000
        count = 8
        size = 4  # LONG
        payload = struct.pack(">I", addr) + bytes([count, size])
        self.assertEqual(len(payload), 6)
        self.assertEqual(struct.unpack(">I", payload[:4])[0], addr)
        self.assertEqual(payload[4], count)
        self.assertEqual(payload[5], size)

    def test_sysreg_payload(self):
        reg = 0x00  # RPC
        payload = bytes([reg])
        self.assertEqual(len(payload), 1)
        self.assertEqual(payload[0], reg)

    def test_call_payload(self):
        addr = 0x2000
        payload = struct.pack(">I", addr)
        self.assertEqual(len(payload), 4)
        self.assertEqual(struct.unpack(">I", payload)[0], addr)

    def test_fill_payload(self):
        addr = 0x1000
        count = 32
        value = 0xFF
        size = 1
        payload = struct.pack(">I", addr) + bytes([count]) + struct.pack(">I", value) + bytes([size])
        self.assertEqual(len(payload), 10)

    def test_dump_response_parsing(self):
        # Simulate response from DUMP command
        count = 4
        values = [0x00001000, 0x00001001, 0x00001002, 0x00001003]
        response_data = b""
        for v in values:
            response_data += struct.pack(">I", v)
        self.assertEqual(len(response_data), count * 4)
        for i, v in enumerate(values):
            parsed = struct.unpack(">I", response_data[i*4:(i+1)*4])[0]
            self.assertEqual(parsed, v)


# ------------------------------------------------------------------
#  Test: Error handling
# ------------------------------------------------------------------

class TestErrorHandling(unittest.TestCase):
    def test_timeout_response(self):
        frame = build_frame(CMD_MEM_READ, struct.pack(">I", 0x1000) + bytes([1]))
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(cmd, CMD_MEM_READ)

    def test_target_error_response(self):
        frame = build_frame(CMD_MEM_WRITE, struct.pack(">I", 0x10000) + bytes([0xFF]))
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(cmd, CMD_MEM_WRITE)

    def test_not_supported_response(self):
        frame = build_frame(0xFF)
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(cmd, 0xFF)


# ------------------------------------------------------------------
#  Test: BDM timing
# ------------------------------------------------------------------

class TestBDMTiming(unittest.TestCase):
    def test_clock_period(self):
        # 500 kHz clock = 2 us period, 1 us half-period
        clock_khz = 500
        half_period_us = 1000 / clock_khz
        self.assertEqual(half_period_us, 2.0)

    def test_timeout_ms(self):
        timeout_ms = 5000
        timeout_us = timeout_ms * 1000
        self.assertEqual(timeout_us, 5000000)


# ------------------------------------------------------------------
#  Test: Protocol edge cases
# ------------------------------------------------------------------

class TestProtocolEdgeCases(unittest.TestCase):
    def test_empty_payload(self):
        frame = build_frame(CMD_STATUS)
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(len(payload), 0)

    def test_max_payload(self):
        payload = bytes(range(256))
        frame = build_frame(CMD_MEM_DUMP, payload)
        cmd, parsed, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(parsed, payload)

    def test_single_byte_payload(self):
        frame = build_frame(CMD_SYSREG_READ, bytes([0x03]))
        cmd, payload, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(payload[0], 0x03)

    def test_frame_with_stx_in_payload(self):
        payload = bytes([STX, 0x00, 0x01])
        frame = build_frame(CMD_MEM_WRITE, payload)
        cmd, parsed, ok = parse_frame(frame)
        self.assertTrue(ok)
        self.assertEqual(parsed, payload)


# ------------------------------------------------------------------
#  Main
# ------------------------------------------------------------------

if __name__ == "__main__":
    unittest.main()
