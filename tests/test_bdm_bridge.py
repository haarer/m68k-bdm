#!/usr/bin/env python3
"""
Test suite for m68k-bdm bridge firmware.
Tests serial protocol communication with the BDM bridge on Arduino Mega 2560.

Note: Without a connected CPU32 target, BDM-specific commands will return
RSP_TARGET_ERROR, but protocol-level tests should still pass.
"""

import serial
import time
import sys
import os

# Colors
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
NC = '\033[0m'

# Protocol constants from config.h
PROTOCOL_STX = 0x02
PROTOCOL_ETX = 0x03

# Commands
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

# Responses
RSP_OK            = 0x00
RSP_ERROR         = 0x01
RSP_NOT_SUPPORTED = 0x02
RSP_TIMEOUT       = 0x03
RSP_TARGET_ERROR  = 0x04

# Response code has 0x80 ORed in protocol_send_response
RSP_CODE_MASK = 0x7F

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200
TIMEOUT = 2

PASS = 0
FAIL = 0
SERIAL = None


def cleanup():
    """Clean up serial connection."""
    global SERIAL
    if SERIAL:
        try:
            SERIAL.close()
        except Exception:
            pass
        SERIAL = None


def log_pass(test_name):
    global PASS
    print(f"  {test_name}... {GREEN}PASS{NC}")
    PASS += 1


def log_fail(test_name, details=""):
    global FAIL
    print(f"  {test_name}... {RED}FAIL{NC}")
    if details:
        print(f"    {YELLOW}{details}{NC}")
    FAIL += 1


def calculate_checksum(data):
    """Calculate XOR checksum for protocol frame."""
    cs = 0
    for b in data:
        cs ^= b
    return cs


def build_command(cmd, payload=None):
    """Build a protocol command frame."""
    if payload is None:
        payload = []

    frame = bytearray()
    frame.append(PROTOCOL_STX)
    frame.append(cmd)
    frame.append(len(payload))

    cs = PROTOCOL_STX ^ cmd ^ len(payload)
    for b in payload:
        frame.append(b)
        cs ^= b

    frame.append(cs)
    frame.append(PROTOCOL_ETX)
    return bytes(frame)


def parse_response(data):
    """Parse a response frame from the bridge."""
    if len(data) < 5:
        return None, "Response too short"

    if data[0] != PROTOCOL_STX:
        return None, f"Expected STX (0x02), got 0x{data[0]:02X}"
    if data[-1] != PROTOCOL_ETX:
        return None, f"Expected ETX (0x03), got 0x{data[-1]:02X}"

    rsp_code = data[1] & RSP_CODE_MASK
    rsp_len = data[2]

    # Verify checksum (covers STX through payload, matching firmware)
    cs_calc = 0
    for b in data[:-2]:  # Exclude checksum byte and ETX
        cs_calc ^= b

    if cs_calc != data[-2]:
        return None, f"Checksum mismatch: calc 0x{cs_calc:02X}, got 0x{data[-2]:02X}"

    payload = data[3:3+rsp_len] if rsp_len > 0 else bytearray()

    return {
        'code': rsp_code,
        'len': rsp_len,
        'payload': payload
    }, None


def send_command(cmd, payload=None, timeout=TIMEOUT):
    """Send a command and read the response."""
    global SERIAL

    frame = build_command(cmd, payload)
    SERIAL.write(frame)
    SERIAL.flush()

    # Read response
    response = bytearray()
    start_time = time.time()

    while time.time() - start_time < timeout:
        if SERIAL.in_waiting:
            byte = SERIAL.read(1)
            if byte:
                response.append(byte[0])
                # Check if we have a complete frame
                if len(response) >= 2 and response[-1] == PROTOCOL_ETX:
                    # Try to parse
                    rsp, err = parse_response(response)
                    if rsp or err:
                        return rsp, err, bytes(response)
        time.sleep(0.01)

    return None, "Timeout waiting for response", bytes(response)


def test_serial_connection():
    """Test basic serial connection to the bridge."""
    global SERIAL

    try:
        SERIAL = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT)
        time.sleep(2)  # Wait for Arduino to reset after DTR toggle
        SERIAL.reset_input_buffer()
        return True
    except Exception as e:
        log_fail("serial_connection", str(e))
        return False


def test_status_command():
    """Test STATUS command (should work without target)."""
    _, err, raw = send_command(CMD_STATUS)

    if err:
        log_fail("status_command", err)
        return False

    log_pass("status_command")
    return True


def test_invalid_command():
    """Test that invalid commands return RSP_NOT_SUPPORTED."""
    rsp, err, raw = send_command(0xFF)

    if err:
        log_fail("invalid_command", err)
        return False

    if rsp and rsp['code'] == RSP_NOT_SUPPORTED:
        log_pass("invalid_command")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("invalid_command", f"Expected RSP_NOT_SUPPORTED (0x02), got 0x{code:02X}")
        return False


def test_mem_read_no_target():
    """Test MEM_READ without target (should return RSP_TARGET_ERROR)."""
    # Address 0x1000, count 4
    payload = [0x00, 0x00, 0x10, 0x00, 0x04]
    rsp, err, raw = send_command(CMD_MEM_READ, payload)

    if err:
        log_fail("mem_read_no_target", err)
        return False

    if rsp and rsp['code'] == RSP_TARGET_ERROR:
        log_pass("mem_read_no_target")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("mem_read_no_target", f"Expected RSP_TARGET_ERROR (0x04), got 0x{code:02X}")
        return False


def test_mem_write_no_target():
    """Test MEM_WRITE without target (should return RSP_TARGET_ERROR)."""
    # Address 0x1000, data 0xDEADBEEF
    payload = [0x00, 0x00, 0x10, 0x00, 0xDE, 0xAD, 0xBE, 0xEF]
    rsp, err, raw = send_command(CMD_MEM_WRITE, payload)

    if err:
        log_fail("mem_write_no_target", err)
        return False

    if rsp and rsp['code'] == RSP_TARGET_ERROR:
        log_pass("mem_write_no_target")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("mem_write_no_target", f"Expected RSP_TARGET_ERROR (0x04), got 0x{code:02X}")
        return False


def test_reg_read_no_target():
    """Test REG_READ without target (should return RSP_TARGET_ERROR)."""
    # Register 0 (D0)
    payload = [0x00]
    rsp, err, raw = send_command(CMD_REG_READ, payload)

    if err:
        log_fail("reg_read_no_target", err)
        return False

    if rsp and rsp['code'] == RSP_TARGET_ERROR:
        log_pass("reg_read_no_target")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("reg_read_no_target", f"Expected RSP_TARGET_ERROR (0x04), got 0x{code:02X}")
        return False


def test_reg_write_no_target():
    """Test REG_WRITE without target (should return RSP_TARGET_ERROR)."""
    # Register 0 (D0), value 0x12345678
    payload = [0x00, 0x12, 0x34, 0x56, 0x78]
    rsp, err, raw = send_command(CMD_REG_WRITE, payload)

    if err:
        log_fail("reg_write_no_target", err)
        return False

    if rsp and rsp['code'] == RSP_TARGET_ERROR:
        log_pass("reg_write_no_target")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("reg_write_no_target", f"Expected RSP_TARGET_ERROR (0x04), got 0x{code:02X}")
        return False


def test_target_reset_no_target():
    """Test TARGET_RESET without target (should return RSP_TARGET_ERROR)."""
    rsp, err, raw = send_command(CMD_TARGET_RESET)

    if err:
        log_fail("target_reset_no_target", err)
        return False

    if rsp and rsp['code'] == RSP_TARGET_ERROR:
        log_pass("target_reset_no_target")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("target_reset_no_target", f"Expected RSP_TARGET_ERROR (0x04), got 0x{code:02X}")
        return False


def test_target_halt_no_target():
    """Test TARGET_HALT without target (should return RSP_TARGET_ERROR)."""
    rsp, err, raw = send_command(CMD_TARGET_HALT)

    if err:
        log_fail("target_halt_no_target", err)
        return False

    if rsp and rsp['code'] == RSP_TARGET_ERROR:
        log_pass("target_halt_no_target")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("target_halt_no_target", f"Expected RSP_TARGET_ERROR (0x04), got 0x{code:02X}")
        return False


def test_bad_checksum():
    """Test that bad checksum is rejected."""
    # Build a command with wrong checksum
    frame = bytearray()
    frame.append(PROTOCOL_STX)
    frame.append(CMD_STATUS)
    frame.append(0x00)
    frame.append(0xFF)  # Wrong checksum
    frame.append(PROTOCOL_ETX)

    SERIAL.write(bytes(frame))
    SERIAL.flush()

    # Wait a bit and check if there's any response
    time.sleep(0.5)
    response = SERIAL.read_all()

    if len(response) == 0:
        log_pass("bad_checksum_rejected")
        return True
    else:
        log_fail("bad_checksum_rejected", f"Got unexpected response: {response.hex()}")
        return False


def test_incomplete_frame():
    """Test that incomplete frames are handled gracefully."""
    # Send only STX and command, no ETX
    frame = bytes([PROTOCOL_STX, CMD_STATUS, 0x00])

    SERIAL.write(frame)
    SERIAL.flush()

    # Now send a proper command
    time.sleep(0.5)
    rsp, err, raw = send_command(CMD_STATUS)

    if err:
        log_fail("incomplete_frame", err)
        return False

    if rsp and rsp['code'] == RSP_OK:
        log_pass("incomplete_frame")
        return True
    else:
        log_fail("incomplete_frame", "Bridge did not recover from incomplete frame")
        return False


def test_config_command():
    """Test CONFIG command (should work without target)."""
    rsp, err, raw = send_command(CMD_CONFIG)

    if err:
        log_fail("config_command", err)
        return False

    if rsp and rsp['code'] == RSP_OK:
        log_pass("config_command")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("config_command", f"Expected RSP_OK (0x00), got 0x{code:02X}")
        return False


def test_mem_read_valid_address_format():
    """Test MEM_READ with valid address format (protocol level)."""
    # Just check the command is properly formed and bridge responds
    # Address 0x00000000, count 1
    payload = [0x00, 0x00, 0x00, 0x00, 0x01]
    rsp, err, raw = send_command(CMD_MEM_READ, payload)

    if err:
        log_fail("mem_read_valid_format", err)
        return False

    # Should get either OK or TARGET_ERROR (both mean protocol works)
    if rsp and rsp['code'] in (RSP_OK, RSP_TARGET_ERROR):
        log_pass("mem_read_valid_format")
        return True
    else:
        code = rsp['code'] if rsp else 'None'
        log_fail("mem_read_valid_format", f"Unexpected response code 0x{code:02X}")
        return False


def test_response_format():
    """Test that response format matches protocol specification."""
    rsp, err, raw = send_command(CMD_STATUS)

    if err:
        log_fail("response_format", err)
        return False

    # Check response structure
    if len(raw) < 5:
        log_fail("response_format", f"Response too short: {len(raw)} bytes")
        return False

    if raw[0] != PROTOCOL_STX:
        log_fail("response_format", f"Response missing STX")
        return False

    if raw[-1] != PROTOCOL_ETX:
        log_fail("response_format", f"Response missing ETX")
        return False

    # Check that response code has 0x80 bit set (from protocol_send_response)
    if not (raw[1] & 0x80):
        log_fail("response_format", f"Response code missing 0x80 bit")
        return False

    log_pass("response_format")
    return True


def main():
    global PASS, FAIL, SERIAL

    print("=" * 60)
    print("m68k-bdm Bridge Test Suite (Serial /dev/ttyACM0)")
    print("=" * 60)
    print()

    # Check if pyserial is available
    try:
        import serial
    except ImportError:
        print(f"{RED}Error: pyserial not found{NC}")
        print("Install with: pip install pyserial")
        print("Or: python3 -m venv /tmp/venv && . /tmp/venv/bin/activate && pip install pyserial")
        sys.exit(1)

    # Check if serial port exists
    if not os.path.exists(SERIAL_PORT):
        print(f"{RED}Error: {SERIAL_PORT} not found{NC}")
        print("Make sure Arduino is connected")
        sys.exit(1)

    # Connect to bridge
    print(f"Connecting to BDM bridge at {SERIAL_PORT}...")
    if not test_serial_connection():
        sys.exit(1)
    print("Connected successfully")
    print()

    print("Running tests...")
    print()

    try:
        # Protocol-level tests (should work without target)
        test_response_format()
        test_status_command()
        test_config_command()
        test_invalid_command()
        test_bad_checksum()
        test_incomplete_frame()

        # Validity tests
        test_mem_read_valid_address_format()

        # Target-dependent tests (should return TARGET_ERROR without target)
        test_mem_read_no_target()
        test_mem_write_no_target()
        test_reg_read_no_target()
        test_reg_write_no_target()
        test_target_reset_no_target()
        test_target_halt_no_target()

    finally:
        cleanup()

    # Summary
    print()
    print("=" * 60)
    print("Test Results:")
    print(f"  {GREEN}Passed: {PASS}{NC}")
    print(f"  {RED}Failed: {FAIL}{NC}")
    print("=" * 60)

    if FAIL == 0:
        print(f"{GREEN}All tests passed!{NC}")
        sys.exit(0)
    else:
        print(f"{RED}Some tests failed!{NC}")
        sys.exit(1)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        cleanup()
        sys.exit(1)
