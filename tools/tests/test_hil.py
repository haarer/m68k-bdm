#!/usr/bin/env python3
"""
Hardware-in-the-Loop (HIL) test suite for m68k-bdm bridge firmware.

Communicates with the bridge via USB CDC serial, sends BDM commands,
and validates responses against a CPU32 target simulator running on
separate hardware.

Usage:
    python3 test_hil.py [--port /dev/ttyACM0]
"""

import argparse
import serial
import struct
import sys
import time

# ------------------------------------------------------------------ #
#  Protocol Constants                                                  #
# ------------------------------------------------------------------ #

STX = 0x02
ETX = 0x03

# Commands
CMD_BDM_ENABLE     = 0x10
CMD_MEM_READ       = 0x11
CMD_MEM_WRITE      = 0x12
CMD_REG_READ       = 0x13
CMD_REG_WRITE      = 0x14
CMD_TARGET_RESET   = 0x15
CMD_TARGET_HALT    = 0x16
CMD_TARGET_GO      = 0x17
CMD_STEP           = 0x18
CMD_BREAKPOINT_SET = 0x19
CMD_BREAKPOINT_CLR = 0x1A
CMD_STATUS         = 0x1B
CMD_CONFIG         = 0x1C
CMD_SYSREG_READ    = 0x1D
CMD_SYSREG_WRITE   = 0x1E
CMD_MEM_DUMP       = 0x1F
CMD_MEM_FILL       = 0x20
CMD_CALL           = 0x21

# Responses
RSP_OK            = 0x00
RSP_ERROR         = 0x01
RSP_NOT_SUPPORTED = 0x02
RSP_TIMEOUT       = 0x03
RSP_TARGET_ERROR  = 0x04

# System register select codes
SR_RPC   = 0x00
SR_PCC   = 0x01
SR_ATEMP = 0x02
SR_FAR   = 0x03
SR_VBR   = 0x04
SR_SR    = 0x05
SR_USP   = 0x06
SR_SSP   = 0x07
SR_SFC   = 0x08
SR_DFC   = 0x09

# ------------------------------------------------------------------ #
#  Bridge Client                                                       #
# ------------------------------------------------------------------ #

class BridgeClient:
    """Communicates with the BDM bridge via serial."""

    def __init__(self, port, baud=115200, timeout=1.0):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        time.sleep(0.5)  # Wait for USB CDC enumeration

    def _build_frame(self, cmd, payload=b''):
        """Build a protocol frame: STX CMD LEN PAYLOAD CS ETX"""
        frame = bytes([STX, cmd, len(payload)]) + payload
        cs = 0
        for b in frame:
            cs ^= b
        frame += bytes([cs, ETX])
        return frame

    def _parse_response(self):
        """Parse a response frame, return (code, payload) or raise."""
        # Wait for STX
        while True:
            b = self.ser.read(1)
            if not b:
                raise TimeoutError("No response from bridge")
            if b[0] == STX:
                break

        # Read code
        code_byte = self.ser.read(1)
        if not code_byte:
            raise TimeoutError("Incomplete response")
        code = code_byte[0] & 0x7F  # Strip bit 7

        # Read length
        len_byte = self.ser.read(1)
        if not len_byte:
            raise TimeoutError("Incomplete response")
        length = len_byte[0]

        # Read payload
        payload = self.ser.read(length) if length > 0 else b''
        if len(payload) < length:
            raise TimeoutError("Incomplete payload")

        # Read checksum
        cs_byte = self.ser.read(1)
        if not cs_byte:
            raise TimeoutError("Missing checksum")

        # Read ETX
        etx_byte = self.ser.read(1)
        if not etx_byte or etx_byte[0] != ETX:
            raise ValueError(f"Missing ETX, got {etx_byte}")

        # Verify checksum
        cs = STX ^ code_byte[0] ^ length
        for b in payload:
            cs ^= b
        if cs != cs_byte[0]:
            raise ValueError(f"Checksum mismatch: expected {cs:#04x}, got {cs_byte[0]:#04x}")

        return code, payload

    def send_command(self, cmd, payload=b''):
        """Send a command and return (code, payload)."""
        frame = self._build_frame(cmd, payload)
        self.ser.write(frame)
        self.ser.flush()
        return self._parse_response()

    def u32(self, value):
        """Pack a 32-bit value to bytes (big-endian)."""
        return struct.pack('>I', value)

    def unpack_u32(self, data):
        """Unpack 4 bytes to a 32-bit value (big-endian)."""
        return struct.unpack('>I', data)[0]

    # ---- High-level BDM operations ----

    def bdm_enable(self):
        """Enable BDM mode on target."""
        code, payload = self.send_command(CMD_BDM_ENABLE)
        return code == RSP_OK and payload == b'\x01'

    def status(self):
        """Query BDM status. Returns True if in BDM mode."""
        code, payload = self.send_command(CMD_STATUS)
        return code == RSP_OK and payload == b'\x01'

    def reg_read(self, reg):
        """Read a data/address register (0-7 = D0-D7, 8-15 = A0-A7)."""
        code, payload = self.send_command(CMD_REG_READ, bytes([reg]))
        if code != RSP_OK:
            raise RuntimeError(f"reg_read({reg}) failed: code={code:#04x}")
        return self.unpack_u32(payload)

    def reg_write(self, reg, value):
        """Write a data/address register."""
        payload = bytes([reg]) + self.u32(value)
        code, _ = self.send_command(CMD_REG_WRITE, payload)
        return code == RSP_OK

    def sysreg_read(self, select):
        """Read a system register."""
        code, payload = self.send_command(CMD_SYSREG_READ, bytes([select]))
        if code != RSP_OK:
            raise RuntimeError(f"sysreg_read({select}) failed: code={code:#04x}")
        return self.unpack_u32(payload)

    def sysreg_write(self, select, value):
        """Write a system register."""
        payload = bytes([select]) + self.u32(value)
        code, _ = self.send_command(CMD_SYSREG_WRITE, payload)
        return code == RSP_OK

    def mem_read(self, addr, count, size=1):
        """Read memory. size: 1=byte, 2=word, 4=long."""
        payload = self.u32(addr) + bytes([count, size])
        code, data = self.send_command(CMD_MEM_READ, payload)
        if code != RSP_OK:
            raise RuntimeError(f"mem_read({addr:#08x}) failed: code={code:#04x}")
        return list(data[:count])

    def mem_write(self, addr, data):
        """Write memory (byte writes)."""
        payload = self.u32(addr) + bytes(data)
        code, _ = self.send_command(CMD_MEM_WRITE, payload)
        return code == RSP_OK

    def mem_dump(self, addr, count, size=1):
        """Dump memory (bulk read with auto-increment)."""
        payload = self.u32(addr) + bytes([count, size])
        code, data = self.send_command(CMD_MEM_DUMP, payload)
        if code != RSP_OK:
            raise RuntimeError(f"mem_dump({addr:#08x}) failed: code={code:#04x}")
        values = []
        for i in range(count):
            if size == 4:
                values.append(self.unpack_u32(data[i*4:(i+1)*4]))
            elif size == 2:
                values.append(struct.unpack('>H', data[i*2:(i+1)*2])[0])
            else:
                values.append(data[i])
        return values

    def mem_fill(self, addr, value, count, size=1):
        """Fill memory (bulk write with auto-increment)."""
        payload = self.u32(addr) + self.u32(value) + bytes([count, size])
        code, _ = self.send_command(CMD_MEM_FILL, payload)
        return code == RSP_OK

    def target_reset(self):
        """Reset target."""
        code, _ = self.send_command(CMD_TARGET_RESET)
        return code == RSP_OK

    def target_go(self):
        """Resume target execution."""
        code, _ = self.send_command(CMD_TARGET_GO)
        return code == RSP_OK

    def call(self, addr):
        """Call code at address."""
        code, _ = self.send_command(CMD_CALL, self.u32(addr))
        return code == RSP_OK

    def close(self):
        self.ser.close()


# ------------------------------------------------------------------ #
#  Test Framework                                                      #
# ------------------------------------------------------------------ #

class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []

    def ok(self, name):
        self.passed += 1
        print(f"  PASS  {name}")

    def fail(self, name, reason=""):
        self.failed += 1
        msg = f"  FAIL  {name}"
        if reason:
            msg += f": {reason}"
        print(msg)
        self.errors.append((name, reason))

    def summary(self):
        total = self.passed + self.failed
        print(f"\n{'='*50}")
        print(f"Results: {self.passed}/{total} passed, {self.failed} failed")
        if self.errors:
            print("\nFailures:")
            for name, reason in self.errors:
                print(f"  - {name}: {reason}")
        return self.failed == 0


# ------------------------------------------------------------------ #
#  Test Cases                                                          #
# ------------------------------------------------------------------ #

def test_bdm_entry(r, bridge):
    """Test BDM entry sequence."""
    # Enable BDM
    ok = bridge.bdm_enable()
    if ok:
        r.ok("BDM enable")
    else:
        r.fail("BDM enable", "target did not enter BDM mode")
        return

    # Check status
    if bridge.status():
        r.ok("BDM status after enable")
    else:
        r.fail("BDM status after enable", "status reports not in BDM mode")


def test_data_registers(r, bridge):
    """Test data register read/write."""
    if not bridge.status():
        r.fail("D register read/write", "not in BDM mode")
        return

    # Write and read back each data register
    for reg in range(8):
        test_val = 0xDEADBEEF + reg
        if bridge.reg_write(reg, test_val):
            val = bridge.reg_read(reg)
            if val == test_val:
                r.ok(f"D{reg} read/write (0x{test_val:08X})")
            else:
                r.fail(f"D{reg} read/write", f"expected 0x{test_val:08X}, got 0x{val:08X}")
        else:
            r.fail(f"D{reg} write", "write command failed")


def test_address_registers(r, bridge):
    """Test address register read/write."""
    if not bridge.status():
        r.fail("A register read/write", "not in BDM mode")
        return

    for reg in range(8):
        test_val = 0xCAFEBABE + reg
        if bridge.reg_write(reg + 8, test_val):
            val = bridge.reg_read(reg + 8)
            if val == test_val:
                r.ok(f"A{reg} read/write (0x{test_val:08X})")
            else:
                r.fail(f"A{reg} read/write", f"expected 0x{test_val:08X}, got 0x{val:08X}")
        else:
            r.fail(f"A{reg} write", "write command failed")


def test_system_registers(r, bridge):
    """Test system register read/write."""
    if not bridge.status():
        r.fail("system register read/write", "not in BDM mode")
        return

    # Test VBR
    test_val = 0x00001000
    if bridge.sysreg_write(SR_VBR, test_val):
        val = bridge.sysreg_read(SR_VBR)
        if val == test_val:
            r.ok(f"VBR read/write (0x{test_val:08X})")
        else:
            r.fail("VBR read/write", f"expected 0x{test_val:08X}, got 0x{val:08X}")
    else:
        r.fail("VBR write", "write command failed")

    # Test ATEMP (should be 0x00000002 for external BKPT entry)
    val = bridge.sysreg_read(SR_ATEMP)
    if val == 0x00000002:
        r.ok("ATEMP entry source (0x00000002)")
    else:
        r.fail("ATEMP entry source", f"expected 0x00000002, got 0x{val:08X}")


def test_memory_byte(r, bridge):
    """Test byte memory read/write."""
    if not bridge.status():
        r.fail("memory byte access", "not in BDM mode")
        return

    addr = 0x0100
    test_data = [0xAA, 0xBB, 0xCC, 0xDD, 0xEE]

    if bridge.mem_write(addr, test_data):
        data = bridge.mem_read(addr, len(test_data), size=1)
        if data == test_data:
            r.ok("memory byte read/write")
        else:
            r.fail("memory byte read/write", f"expected {test_data}, got {data}")
    else:
        r.fail("memory byte write", "write command failed")


def test_memory_word(r, bridge):
    """Test word memory read/write."""
    if not bridge.status():
        r.fail("memory word access", "not in BDM mode")
        return

    addr = 0x0200
    test_val = 0xBEEF

    # Write word via two byte writes
    bridge.mem_write(addr, [0xBE, 0xEF])
    data = bridge.mem_read(addr, 1, size=2)
    if data == [test_val]:
        r.ok("memory word read")
    else:
        r.fail("memory word read", f"expected [{test_val:#06x}], got {data}")


def test_memory_long(r, bridge):
    """Test long memory read/write."""
    if not bridge.status():
        r.fail("memory long access", "not in BDM mode")
        return

    addr = 0x0300
    test_val = 0xDEADBEEF

    # Write long via four byte writes
    bridge.mem_write(addr, [0xDE, 0xAD, 0xBE, 0xEF])
    data = bridge.mem_read(addr, 1, size=4)
    if data == [test_val]:
        r.ok("memory long read")
    else:
        r.fail("memory long read", f"expected [{test_val:#010x}], got {data}")


def test_memory_dump(r, bridge):
    """Test bulk memory dump."""
    if not bridge.status():
        r.fail("memory dump", "not in BDM mode")
        return

    addr = 0x0400
    test_data = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88]
    bridge.mem_write(addr, test_data)

    values = bridge.mem_dump(addr, len(test_data), size=1)
    if values == test_data:
        r.ok("memory dump (byte)")
    else:
        r.fail("memory dump (byte)", f"expected {test_data}, got {values}")


def test_memory_fill(r, bridge):
    """Test bulk memory fill."""
    if not bridge.status():
        r.fail("memory fill", "not in BDM mode")
        return

    addr = 0x0500
    fill_val = 0xAB
    count = 8
    bridge.mem_fill(addr, fill_val, count, size=1)

    data = bridge.mem_read(addr, count, size=1)
    expected = [fill_val] * count
    if data == expected:
        r.ok("memory fill (byte)")
    else:
        r.fail("memory fill (byte)", f"expected {expected}, got {data}")


def test_control_commands(r, bridge):
    """Test GO, NOP, and status commands."""
    if not bridge.status():
        r.fail("control commands", "not in BDM mode")
        return

    # Status command
    code, payload = bridge.send_command(CMD_STATUS)
    if code == RSP_OK:
        r.ok("STATUS command")
    else:
        r.fail("STATUS command", f"code={code:#04x}")

    # Config command
    code, payload = bridge.send_command(CMD_CONFIG)
    if code == RSP_OK:
        r.ok("CONFIG command")
    else:
        r.fail("CONFIG command", f"code={code:#04x}")


def test_rapid_commands(r, bridge):
    """Test rapid-fire command handling."""
    if not bridge.status():
        r.fail("rapid commands", "not in BDM mode")
        return

    success = 0
    for i in range(10):
        try:
            val = bridge.reg_read(0)
            success += 1
        except Exception:
            pass

    if success == 10:
        r.ok("rapid commands (10/10)")
    else:
        r.fail("rapid commands", f"{success}/10 succeeded")


# ------------------------------------------------------------------ #
#  Main                                                                #
# ------------------------------------------------------------------ #

def main():
    parser = argparse.ArgumentParser(description="HIL test suite for m68k-bdm bridge")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Serial port for bridge")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()

    print(f"Connecting to bridge on {args.port}...")
    try:
        bridge = BridgeClient(args.port, args.baud)
    except serial.SerialException as e:
        print(f"Failed to open serial port: {e}")
        sys.exit(1)

    r = TestResult()

    print("\n--- BDM Entry ---")
    test_bdm_entry(r, bridge)

    print("\n--- Data Registers ---")
    test_data_registers(r, bridge)

    print("\n--- Address Registers ---")
    test_address_registers(r, bridge)

    print("\n--- System Registers ---")
    test_system_registers(r, bridge)

    print("\n--- Memory Access ---")
    test_memory_byte(r, bridge)
    test_memory_word(r, bridge)
    test_memory_long(r, bridge)

    print("\n--- Bulk Memory Operations ---")
    test_memory_dump(r, bridge)
    test_memory_fill(r, bridge)

    print("\n--- Control Commands ---")
    test_control_commands(r, bridge)

    print("\n--- Protocol Edge Cases ---")
    test_rapid_commands(r, bridge)

    ok = r.summary()
    bridge.close()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
