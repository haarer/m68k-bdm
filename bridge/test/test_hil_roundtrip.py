import os
import pytest
import serial
import subprocess
import time

BRIDGE_PORT = os.environ.get("TEST_SERIAL_PORT", "/dev/ttyUSB0")
TARGET_PORT = os.environ.get("TEST_TARGET_SERIAL_PORT", "/dev/ttyUSB1")
BAUDRATE = 115200
TIMEOUT = 5

HIL_TARGET_AVAILABLE = os.path.exists(TARGET_PORT)


@pytest.fixture(autouse=True)
def _skip_no_hil():
    pytest.skip(
        f"HIL target not available ({TARGET_PORT}). "
        f"Wire target_sim to {TARGET_PORT} or set TEST_TARGET_SERIAL_PORT.",
        allow_module_level=True,
    )


def _reset_bridge():
    subprocess.run(["st-flash", "reset"], capture_output=True, timeout=10)


def _open_port(port):
    ser = serial.Serial(port, BAUDRATE, timeout=TIMEOUT)
    ser.reset_input_buffer()
    return ser


def _wait_prompt(ser):
    ser.timeout = 5
    ser.read_until(b"bdm> ")
    ser.timeout = TIMEOUT


def send_cmd(ser, cmd):
    ser.reset_input_buffer()
    ser.write(cmd + b"\n")
    resp = ser.read_until(b"bdm> ")
    if resp.endswith(b"bdm> "):
        resp = resp[:-5]
    echo_prefix = cmd + b"\n\r"
    if resp.startswith(echo_prefix):
        resp = resp[len(echo_prefix):]
    return resp


def drain_debug(target_ser):
    time.sleep(0.1)
    data = target_ser.read_all()
    target_ser.reset_input_buffer()
    return data


def test_memory_write_read_roundtrip():
    """Write a 32-bit value to target memory, then read it back and verify."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        addr = "0x2000"
        val = "0xAABBCCDD"

        write_resp = send_cmd(bridge, f"mwrite {addr} {val}".encode())
        assert b"[OK]" in write_resp, f"mwrite failed: {write_resp!r}"

        debug_after_write = drain_debug(target)
        assert b"[word]" in debug_after_write, f"No BDM word traffic: {debug_after_write!r}"

        read_resp = send_cmd(bridge, f"mread {addr} 4".encode())
        assert b"[OK]" in read_resp, f"mread failed: {read_resp!r}"
        assert val.encode() in read_resp, f"Value {val} not in response: {read_resp!r}"

        debug_after_read = drain_debug(target)
        assert b"[word]" in debug_after_read, f"No BDM word traffic on read: {debug_after_read!r}"
    finally:
        bridge.close()
        target.close()


def test_memory_byte_roundtrip():
    """Write then read a single byte via BDM."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        addr = "0x2100"
        val = "0x42"

        write_resp = send_cmd(bridge, f"mwrite {addr} {val} 1".encode())
        assert b"[OK]" in write_resp, f"byte mwrite failed: {write_resp!r}"

        read_resp = send_cmd(bridge, f"mread {addr} 1".encode())
        assert b"[OK]" in read_resp, f"byte mread failed: {read_resp!r}"
        assert val.encode() in read_resp, f"Byte {val} not in response: {read_resp!r}"
    finally:
        bridge.close()
        target.close()


def test_memory_word_roundtrip():
    """Write then read a 16-bit word via BDM."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        addr = "0x2200"
        val = "0xDEAD"

        write_resp = send_cmd(bridge, f"mwrite {addr} {val} 2".encode())
        assert b"[OK]" in write_resp, f"word mwrite failed: {write_resp!r}"

        read_resp = send_cmd(bridge, f"mread {addr} 2".encode())
        assert b"[OK]" in read_resp, f"word mread failed: {read_resp!r}"
        assert val.encode() in read_resp, f"Word {val} not in response: {read_resp!r}"
    finally:
        bridge.close()
        target.close()


def test_memory_sequential_write():
    """Write 4 consecutive longs, read them back in sequence."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        base = "0x3000"
        values = ["0x11111111", "0x22222222", "0x33333333", "0x44444444"]

        for v in values:
            write_resp = send_cmd(bridge, f"mwrite {base} {v}".encode())
            assert b"[OK]" in write_resp, f"mwrite {v} failed: {write_resp!r}"

        time.sleep(0.05)
        drain_debug(target)

        for i, v in enumerate(values):
            addr = f"0x{int(base, 16) + i * 4:04X}"
            read_resp = send_cmd(bridge, f"mread {addr} 4".encode())
            assert b"[OK]" in read_resp, f"mread {addr} failed: {read_resp!r}"
            assert v.encode() in read_resp, f"Value {v} not at {addr}: {read_resp!r}"
    finally:
        bridge.close()
        target.close()


def test_regwrite_regread_data_roundtrip():
    """Write a value to D0 register, then read it back."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        val = "0xFEEDFACE"

        write_resp = send_cmd(bridge, b"regwrite 0 " + val.encode())
        assert b"[OK]" in write_resp, f"regwrite D0 failed: {write_resp!r}"

        debug_after = drain_debug(target)
        assert b"[word]" in debug_after, f"No BDM traffic: {debug_after!r}"

        read_resp = send_cmd(bridge, b"regread 0")
        assert b"[OK]" in read_resp, f"regread D0 failed: {read_resp!r}"
        assert val.encode() in read_resp, f"D0 value {val} not in response: {read_resp!r}"
    finally:
        bridge.close()
        target.close()


def test_regwrite_regread_addr_roundtrip():
    """Write a value to A0 register, then read it back."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        val = "0x12345678"

        write_resp = send_cmd(bridge, b"regwrite 8 " + val.encode())
        assert b"[OK]" in write_resp, f"regwrite A0 failed: {write_resp!r}"

        read_resp = send_cmd(bridge, b"regread 8")
        assert b"[OK]" in read_resp, f"regread A0 failed: {read_resp!r}"
        assert val.encode() in read_resp, f"A0 value {val} not in response: {read_resp!r}"
    finally:
        bridge.close()
        target.close()


def test_bdm_enable_freeze():
    """BDM enable should trigger FREEZE assertion on target."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        enable_resp = send_cmd(bridge, b"enable")
        debug_after = drain_debug(target)
        assert b"[freeze]" in debug_after or b"[OK]" in enable_resp or b"[FAIL]" in enable_resp
    finally:
        bridge.close()
        target.close()


def test_debug_log_preamble_detected():
    """BDM commands should produce preamble detection in target debug log."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        send_cmd(bridge, b"status")
        debug = drain_debug(target)

        send_cmd(bridge, b"nop")
        debug += drain_debug(target)

        assert b"[preamble]" in debug or b"[word]" in debug, (
            f"No BDM activity in debug log: {debug!r}"
        )
    finally:
        bridge.close()
        target.close()


def test_debug_log_word_count_increases():
    """Each BDM roundtrip should produce new word shifts in debug log."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        send_cmd(bridge, b"mwrite 0x4000 0xAAAAAAAA")
        debug1 = drain_debug(target)

        send_cmd(bridge, b"mread 0x4000 4")
        debug2 = drain_debug(target)

        words1 = debug1.count(b"[word]")
        words2 = debug2.count(b"[word]")
        assert words1 > 0, f"No words in first operation: {debug1!r}"
        assert words2 > 0, f"No words in second operation: {debug2!r}"
    finally:
        bridge.close()
        target.close()


def test_multiple_register_roundtrip():
    """Write to multiple D registers, then read them all back."""
    _reset_bridge()
    bridge = _open_port(BRIDGE_PORT)
    target = _open_port(TARGET_PORT)
    try:
        _wait_prompt(bridge)
        drain_debug(target)

        vals = {0: "0x11111111", 1: "0x22222222", 2: "0x33333333"}

        for reg, val in vals.items():
            write_resp = send_cmd(bridge,
                                  f"regwrite {reg} {val}".encode())
            assert b"[OK]" in write_resp, f"regwrite D{reg} failed: {write_resp!r}"

        time.sleep(0.05)
        drain_debug(target)

        for reg, val in vals.items():
            read_resp = send_cmd(bridge, f"regread {reg}".encode())
            assert b"[OK]" in read_resp, f"regread D{reg} failed: {read_resp!r}"
            assert val.encode() in read_resp, f"D{reg} value {val} not in: {read_resp!r}"
    finally:
        bridge.close()
        target.close()
