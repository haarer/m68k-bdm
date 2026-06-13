import os
import random
import serial
import subprocess
import time

SERIAL_PORT = os.environ.get("TEST_SERIAL_PORT", "/dev/ttyUSB0")
BAUDRATE = 115200
TIMEOUT = 5


def _ensure_flashed():
    """Flash firmware if not already flashed this session."""
    if not hasattr(_ensure_flashed, "_done"):
        subprocess.run(
            ["st-flash", "--reset", "--format", "ihex", "write", "main.hex"],
            capture_output=True,
            timeout=30,
        )
        time.sleep(1)
        _ensure_flashed._done = True


def _open_serial():
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.reset_input_buffer()
    return ser


def _reset_and_wait_prompt(ser):
    """Reset board and wait for CLI prompt."""
    subprocess.run(["st-flash", "reset"], capture_output=True, timeout=10)
    time.sleep(3)
    # Drain boot output until we see the prompt
    ser.timeout = 5
    data = ser.read_until(b"> ")
    ser.timeout = TIMEOUT
    return data


def send_cmd(ser, cmd):
    """Send command, read until prompt, return response (without prompt)."""
    ser.reset_input_buffer()
    ser.write(cmd + b"\n")
    resp = ser.read_until(b"> ")
    if resp.endswith(b"> "):
        resp = resp[:-2]
    echo_prefix = cmd + b"\n\r"
    if resp.startswith(echo_prefix):
        resp = resp[len(echo_prefix):]
    return resp


def test_ping():
    """Canary: if this fails, board communication is broken."""
    _ensure_flashed()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"ping")
        assert b"pong" in resp, f"No UART communication: {resp!r}"
    finally:
        ser.close()


def test_echobin_small():
    _ensure_flashed()
    size = 50
    data = bytes(random.randrange(256) for _ in range(size))
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        ser.reset_input_buffer()
        ser.write(f"echobin {size}\n".encode())
        ser.read_until(b"ok\n\r")
        ser.write(data)
        echo = ser.read(size)
        ser.read_until(b"> ")
        assert echo == data
    finally:
        ser.close()


def test_echobin_ringbuf_boundary():
    _ensure_flashed()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        for size in [1, 255, 256, 257, 512]:
            data = bytes(random.randrange(256) for _ in range(size))
            ser.reset_input_buffer()
            ser.write(f"echobin {size}\n".encode())
            ser.read_until(b"ok\n\r")
            ser.write(data)
            echo = ser.read(size)
            ser.read_until(b"> ")
            assert echo == data
    finally:
        ser.close()


def test_echobin_all_byte_values():
    _ensure_flashed()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        data = bytes(range(256))
        ser.reset_input_buffer()
        ser.write(b"echobin 256\n")
        ser.read_until(b"ok\n\r")
        ser.write(data)
        echo = ser.read(256)
        ser.read_until(b"> ")
        assert echo == data
    finally:
        ser.close()


def test_pktsend_pattern():
    _ensure_flashed()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        size = 1000
        ser.reset_input_buffer()
        ser.write(f"pktsend {size}\n".encode())
        ser.read_until(b"ok\n\r")
        data = ser.read(size)
        ser.read_until(b"> ")
        expected = bytes(i & 0xFF for i in range(size))
        assert data == expected
    finally:
        ser.close()


def test_rapid_ping():
    _ensure_flashed()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        for _ in range(50):
            ser.write(b"ping\n")
            resp = ser.read_until(b"> ")
            assert b"pong" in resp, f"pong not found in {resp!r}"
    finally:
        ser.close()


def test_mixed_commands():
    _ensure_flashed()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        cmds = [b"ping", b"hello", b"led on", b"led off"]
        ser.reset_input_buffer()
        for cmd in cmds * 5:
            ser.write(cmd + b"\n")
            resp = ser.read_until(b"> ")
            assert b"error" not in resp
    finally:
        ser.close()


def test_consecutive_echobin():
    _ensure_flashed()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        for size in [10, 50, 100]:
            data = bytes(random.randrange(256) for _ in range(size))
            ser.reset_input_buffer()
            ser.write(f"echobin {size}\n".encode())
            ser.read_until(b"ok\n\r")
            ser.write(data)
            echo = ser.read(size)
            ser.read_until(b"> ")
            assert echo == data
    finally:
        ser.close()
