#!/usr/bin/env python3
"""
BDM Bridge Integration Tests
Talks to the flashed AVR bridge over serial and exercises every command.
Without a 68331 target attached, BDM commands return RSP_TARGET_ERROR,
but protocol framing, checksums, and response codes are verified.
"""

import struct
import time
import unittest
import serial

# ------------------------------------------------------------------
#  Protocol constants (must match config.h)
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

ALL_COMMANDS = [
    CMD_MEM_READ, CMD_MEM_WRITE, CMD_REG_READ, CMD_REG_WRITE,
    CMD_TARGET_RESET, CMD_TARGET_HALT, CMD_TARGET_GO, CMD_STEP,
    CMD_BREAKPOINT_SET, CMD_BREAKPOINT_CLR, CMD_STATUS, CMD_CONFIG,
    CMD_SYSREG_READ, CMD_SYSREG_WRITE, CMD_MEM_DUMP, CMD_MEM_FILL,
    CMD_CALL,
]

RSP_OK            = 0x00
RSP_ERROR         = 0x01
RSP_NOT_SUPPORTED = 0x02
RSP_TIMEOUT       = 0x03
RSP_TARGET_ERROR  = 0x04

ALL_RSP_CODES = [RSP_OK, RSP_ERROR, RSP_NOT_SUPPORTED, RSP_TIMEOUT, RSP_TARGET_ERROR]

PORT = "/dev/ttyACM0"
BAUD = 115200

# ------------------------------------------------------------------
#  Frame builder (matches protocol.c state machine exactly)
# ------------------------------------------------------------------

def build_frame(cmd, payload=b""):
    """Build: [STX][CMD][LEN][PAYLOAD...][XOR_CS][ETX]"""
    cs = STX ^ cmd ^ len(payload)
    for b in payload:
        cs ^= b
    return bytes([STX, cmd, len(payload)]) + payload + bytes([cs, ETX])


def xor_checksum(data):
    cs = 0
    for b in data:
        cs ^= b
    return cs


# ------------------------------------------------------------------
#  Serial bridge client
# ------------------------------------------------------------------

class BridgeClient:
    """Wraps pyserial; sends frames and reads responses from the bridge."""

    def __init__(self, port=PORT, baud=BAUD):
        self.port = serial.Serial(port, baud, timeout=2, dsrdtr=True)
        time.sleep(2)  # wait for AVR to boot after DTR reset
        self.port.flushInput()
        self.port.flushOutput()

    def close(self):
        self.port.close()

    def _read_response(self):
        """Read a full response frame and validate framing + checksum."""
        raw = bytearray()
        deadline = time.monotonic() + 2.0

        while time.monotonic() < deadline:
            ch = self.port.read(1)
            if not ch:
                break
            raw.append(ch[0])
            if ch[0] == ETX:
                break

        if len(raw) < 5:
            return None, None, bytes(raw)

        if raw[0] != STX:
            return None, None, bytes(raw)

        if not (raw[1] & 0x80):
            return None, None, bytes(raw)

        rsp_code = raw[1] & 0x7F
        rsp_len = raw[2]
        rsp_payload = bytes(raw[3:3 + rsp_len])
        cs_byte = raw[3 + rsp_len]
        etx_byte = raw[3 + rsp_len + 1]

        if etx_byte != ETX:
            return None, None, bytes(raw)

        expected_cs = STX ^ raw[1] ^ raw[2]
        for b in rsp_payload:
            expected_cs ^= b
        if expected_cs != cs_byte:
            return None, None, bytes(raw)

        return rsp_code, rsp_payload, bytes(raw)

    def send(self, cmd, payload=b""):
        """Send a command frame, read and validate response.
        Returns (rsp_code, rsp_payload) or (None, None) on failure."""
        self.port.flushInput()
        frame = build_frame(cmd, payload)
        self.port.write(frame)
        time.sleep(0.15)
        return self._read_response()[:2]

    # -- Convenience methods --

    def send_status(self):
        return self.send(CMD_STATUS)

    def send_config(self):
        return self.send(CMD_CONFIG)

    def send_mem_read(self, addr, count=1, size=1):
        payload = struct.pack(">I", addr) + bytes([count, size])
        return self.send(CMD_MEM_READ, payload)

    def send_mem_write(self, addr, data):
        payload = struct.pack(">I", addr) + bytes(data)
        return self.send(CMD_MEM_WRITE, payload)

    def send_reg_read(self, reg):
        return self.send(CMD_REG_READ, bytes([reg]))

    def send_reg_write(self, reg, value):
        payload = bytes([reg]) + struct.pack(">I", value)
        return self.send(CMD_REG_WRITE, payload)

    def send_sysreg_read(self, reg):
        return self.send(CMD_SYSREG_READ, bytes([reg]))

    def send_sysreg_write(self, reg, value):
        payload = bytes([reg]) + struct.pack(">I", value)
        return self.send(CMD_SYSREG_WRITE, payload)

    def send_mem_dump(self, addr, count=4, size=4):
        payload = struct.pack(">I", addr) + bytes([count, size])
        return self.send(CMD_MEM_DUMP, payload)

    def send_mem_fill(self, addr, count, value, size=1):
        payload = (struct.pack(">I", addr) + bytes([count]) +
                   struct.pack(">I", value) + bytes([size]))
        return self.send(CMD_MEM_FILL, payload)

    def send_target_reset(self):
        return self.send(CMD_TARGET_RESET)

    def send_target_halt(self):
        return self.send(CMD_TARGET_HALT)

    def send_target_go(self):
        return self.send(CMD_TARGET_GO)

    def send_step(self):
        return self.send(CMD_STEP)

    def send_call(self, addr):
        return self.send(CMD_CALL, struct.pack(">I", addr))

    def send_breakpoint_set(self):
        return self.send(CMD_BREAKPOINT_SET)

    def send_breakpoint_clr(self):
        return self.send(CMD_BREAKPOINT_CLR)

    def send_unknown(self, cmd=0x7F):
        return self.send(cmd)


# ------------------------------------------------------------------
#  Unit tests: frame construction (no serial needed)
# ------------------------------------------------------------------

class TestFrameConstruction(unittest.TestCase):
    """Verify frame building matches protocol.c state machine."""

    def test_empty_payload_frame(self):
        frame = build_frame(CMD_STATUS)
        self.assertEqual(frame[0], STX)
        self.assertEqual(frame[1], CMD_STATUS)
        self.assertEqual(frame[2], 0)
        self.assertEqual(frame[3], STX ^ CMD_STATUS ^ 0)
        self.assertEqual(frame[-1], ETX)
        self.assertEqual(len(frame), 5)

    def test_payload_frame(self):
        payload = struct.pack(">I", 0x00001000) + bytes([4, 4])
        frame = build_frame(CMD_MEM_READ, payload)
        self.assertEqual(frame[0], STX)
        self.assertEqual(frame[1], CMD_MEM_READ)
        self.assertEqual(frame[2], len(payload))
        self.assertEqual(frame[3:3 + len(payload)], payload)
        self.assertEqual(frame[-1], ETX)

    def test_xor_checksum_coverage(self):
        for cmd in ALL_COMMANDS:
            payload = bytes([cmd, 0xAA, 0x55])
            frame = build_frame(cmd, payload)
            cs = STX ^ cmd ^ len(payload)
            for b in payload:
                cs ^= b
            self.assertEqual(frame[-2], cs, f"bad checksum for cmd 0x{cmd:02X}")




# ------------------------------------------------------------------
#  Integration tests: talk to the flashed bridge
# ------------------------------------------------------------------

class TestBridgeIntegration(unittest.TestCase):
    """Sends real frames to the bridge and validates responses."""

    @classmethod
    def setUpClass(cls):
        try:
            cls.bridge = BridgeClient()
            cls.available = True
        except serial.SerialException:
            cls.available = False
            cls.bridge = None

    @classmethod
    def tearDownClass(cls):
        if cls.available and cls.bridge:
            cls.bridge.close()

    def setUp(self):
        if not self.available:
            self.skipTest("serial port not available")

    # -- Non-BDM commands (don't need target) --

    def test_status_returns_ok(self):
        code, payload = self.bridge.send_status()
        self.assertEqual(code, RSP_OK, "STATUS should return RSP_OK")
        self.assertIsNotNone(payload)
        self.assertEqual(len(payload), 1)

    def test_config_returns_ok(self):
        code, _ = self.bridge.send_config()
        self.assertEqual(code, RSP_OK, "CONFIG should return RSP_OK")

    def test_breakpoint_set_returns_ok(self):
        code, _ = self.bridge.send_breakpoint_set()
        self.assertEqual(code, RSP_OK, "BREAKPOINT_SET should return RSP_OK")

    def test_breakpoint_clr_returns_ok(self):
        code, _ = self.bridge.send_breakpoint_clr()
        self.assertEqual(code, RSP_OK, "BREAKPOINT_CLR should return RSP_OK")

    # -- BDM commands (return TARGET_ERROR without 68331) --

    def test_mem_read_returns_target_error(self):
        code, _ = self.bridge.send_mem_read(0x1000, 1, 1)
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_mem_write_returns_target_error(self):
        code, _ = self.bridge.send_mem_write(0x1000, [0xAB, 0xCD])
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_reg_read_returns_target_error(self):
        code, _ = self.bridge.send_reg_read(0)
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_reg_write_returns_target_error(self):
        code, _ = self.bridge.send_reg_write(0, 0x12345678)
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_sysreg_read_returns_target_error(self):
        code, _ = self.bridge.send_sysreg_read(0)
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_sysreg_write_returns_target_error(self):
        code, _ = self.bridge.send_sysreg_write(0, 0x00001000)
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_mem_dump_returns_target_error(self):
        code, _ = self.bridge.send_mem_dump(0x1000, 4, 4)
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_mem_fill_returns_target_error(self):
        code, _ = self.bridge.send_mem_fill(0x1000, 16, 0x55, 1)
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_target_reset_returns_target_error(self):
        code, _ = self.bridge.send_target_reset()
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_target_halt_returns_target_error(self):
        code, _ = self.bridge.send_target_halt()
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_target_go_returns_target_error(self):
        code, _ = self.bridge.send_target_go()
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_step_returns_target_error(self):
        code, _ = self.bridge.send_step()
        self.assertEqual(code, RSP_TARGET_ERROR)

    def test_call_returns_target_error(self):
        code, _ = self.bridge.send_call(0x2000)
        self.assertEqual(code, RSP_TARGET_ERROR)

    # -- Protocol edge cases --

    def test_unknown_command_returns_not_supported(self):
        code, _ = self.bridge.send_unknown(0x7F)
        self.assertEqual(code, RSP_NOT_SUPPORTED)

    def test_response_framing_valid(self):
        """Every response has valid STX, 0x80 bit, XOR checksum, ETX."""
        self.bridge.port.flushInput()
        self.bridge.port.write(build_frame(CMD_STATUS))
        time.sleep(0.15)
        code, payload, raw = self.bridge._read_response()
        self.assertIsNotNone(code, "no response received")
        self.assertLess(code, 0x80, "response code should not have 0x80 bit set")
        self.assertEqual(raw[0], STX)
        self.assertTrue(raw[1] & 0x80)
        self.assertEqual(raw[-1], ETX)

    def test_rapid_commands(self):
        """Send 10 STATUS commands in a row; each should return OK."""
        for _ in range(10):
            code, _ = self.bridge.send_status()
            self.assertEqual(code, RSP_OK, "rapid STATUS should return RSP_OK")

    def test_mem_read_short_payload(self):
        """MEM_READ with too-short payload should return RSP_ERROR."""
        self.bridge.port.flushInput()
        payload = struct.pack(">I", 0x1000)  # missing count and size bytes
        frame = build_frame(CMD_MEM_READ, payload)
        self.bridge.port.write(frame)
        time.sleep(0.15)
        code, _, _ = self.bridge._read_response()
        self.assertEqual(code, RSP_ERROR)


if __name__ == "__main__":
    unittest.main()
