import os
import random
import serial
import subprocess
import time

SERIAL_PORT = os.environ.get("TEST_SERIAL_PORT", "/dev/ttyUSB0")
BAUDRATE = 115200
TIMEOUT = 5


def _ensure_reset():
    if not hasattr(_ensure_reset, "_done"):
        subprocess.run(
            ["st-flash", "reset"],
            capture_output=True,
            timeout=10,
        )
        time.sleep(1)
        _ensure_reset._done = True


def _open_serial():
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.reset_input_buffer()
    return ser


def _reset_and_wait_prompt(ser):
    subprocess.run(["st-flash", "reset"], capture_output=True, timeout=10)
    ser.timeout = 5
    data = ser.read_until(b"bdm> ")
    ser.timeout = TIMEOUT
    return data


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


def test_ping():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"ping")
        assert b"pong" in resp, f"No UART communication: {resp!r}"
    finally:
        ser.close()


def test_help_contains_bdm_commands():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"help")
        for cmd in [b"enable", b"status", b"halt", b"go", b"mread", b"regread", b"call"]:
            assert cmd in resp, f"help missing {cmd.decode()}"
    finally:
        ser.close()


def test_bdm_commands_accepted():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        for cmd in [b"enable", b"status", b"halt", b"go", b"reset", b"step", b"nop"]:
            ser.reset_input_buffer()
            ser.write(cmd + b"\n")
            resp = ser.read_until(b"bdm> ")
            assert b"error: unknown command" not in resp, f"{cmd} was rejected"
            assert b"bdm> " in resp, f"{cmd} did not return prompt"
    finally:
        ser.close()


def test_mread_parsing():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"mread 0x1000 4")
        assert b"ERROR" in resp or b"mread" in resp, f"unexpected: {resp!r}"
    finally:
        ser.close()


def test_regread_parsing():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"regread 0")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_rapid_ping():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        for _ in range(50):
            ser.write(b"ping\n")
            resp = ser.read_until(b"bdm> ")
            assert b"pong" in resp, f"pong not found in {resp!r}"
    finally:
        ser.close()


def test_mixed_commands():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        cmds = [b"ping", b"hello", b"led on", b"led off", b"status", b"nop"]
        ser.reset_input_buffer()
        for cmd in cmds * 5:
            ser.write(cmd + b"\n")
            resp = ser.read_until(b"bdm> ")
            assert b"error" not in resp or b"error: unknown" not in resp
    finally:
        ser.close()
