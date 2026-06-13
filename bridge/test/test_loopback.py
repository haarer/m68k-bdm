import os
import serial
import subprocess

SERIAL_PORT = os.environ.get("TEST_SERIAL_PORT", "/dev/ttyUSB0")
BAUDRATE = 115200
TIMEOUT = 5
EXPECTED_BOOT = b"hello world\n\rstdio connected via interrupt-based UART with ring buffers\n\r> "


def reset_target():
    subprocess.run(["st-flash", "reset"], capture_output=True)


def send_cmd(ser, cmd):
    ser.reset_input_buffer()
    ser.write(cmd + b"\n")
    resp = ser.read_until(b"> ")
    if resp.endswith(b"> "):
        resp = resp[:-2]
    echo_prefix = cmd + b"\n\r"
    if resp.startswith(echo_prefix):
        resp = resp[len(echo_prefix):]
    return resp


def test_boot_message():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    data = ser.read_until(b"> ")
    ser.close()
    assert data == EXPECTED_BOOT, f"Expected {EXPECTED_BOOT!r}, got {data!r}"


def test_hello_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, b"hello")
    ser.close()
    assert resp == b"hello world\n\r", f"Expected hello world, got {resp!r}"


def test_echo_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, b"echo HELLOCLI")
    ser.close()
    assert resp == b"HELLOCLI\n\r", f"Expected HELLOCLI, got {resp!r}"


def test_led_on_off():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, b"led on")
    assert resp == b"ok\n\r"
    resp = send_cmd(ser, b"led off")
    assert resp == b"ok\n\r"
    ser.close()


def test_help_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, b"help")
    ser.close()
    assert b"available commands:" in resp
    assert b"help" in resp
    assert b"hello" in resp
    assert b"led on" in resp
    assert b"led off" in resp
    assert b"echo" in resp


def test_unknown_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, b"notacommand")
    ser.close()
    assert b"error: unknown command" in resp


def test_ping():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, b"ping")
    ser.close()
    assert resp == b"pong\n\r", f"Expected pong, got {resp!r}"


def test_empty_line():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    ser.reset_input_buffer()
    ser.write(b"\n")
    resp = ser.read_until(b"> ")
    ser.close()
    assert resp == b"\n\r> ", f"Expected echo + prompt, got {resp!r}"


def test_echobin_small():
    n = 100
    cmd = f"echobin {n}".encode()
    data = bytes(range(n))

    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    ser.reset_input_buffer()
    ser.write(cmd + b"\n")
    ser.write(data)
    resp = ser.read_until(b"> ")
    ser.close()

    if resp.endswith(b"> "):
        resp = resp[:-2]
    echo = cmd + b"\n\r"
    if resp.startswith(echo):
        resp = resp[len(echo):]
    assert resp.startswith(b"ok\n\r"), f"Expected ok prefix, got {resp[:20]!r}"
    resp = resp[4:]
    assert resp == data, f"Expected {n} echoed bytes, got {len(resp)}"


def test_pktsend_small():
    n = 10
    cmd = f"pktsend {n}".encode()
    expected = bytes(i & 0xFF for i in range(n))

    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, cmd)
    ser.close()
    assert resp.startswith(b"ok\n\r"), f"Expected ok, got {resp[:20]!r}"
    data = resp[4:]
    assert data == expected, f"Expected {len(expected)} bytes, got {len(data)}"


def test_pktsend_buffer_fill():
    n = 255
    cmd = f"pktsend {n}".encode()
    expected = bytes(i & 0xFF for i in range(n))

    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, cmd)
    ser.close()
    assert resp.startswith(b"ok\n\r"), f"Expected ok, got {resp[:20]!r}"
    data = resp[4:]
    assert data == expected, f"Expected {len(expected)} bytes, got {len(data)}"


def test_pktsend_exceed_buffer():
    n = 256
    cmd = f"pktsend {n}".encode()
    expected = bytes(i & 0xFF for i in range(n))

    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, cmd)
    ser.close()
    assert resp.startswith(b"ok\n\r"), f"Expected ok, got {resp[:20]!r}"
    data = resp[4:]
    assert data == expected, f"Expected {len(expected)} bytes, got {len(data)}"


def test_pktsend_large():
    n = 512
    cmd = f"pktsend {n}".encode()
    expected = bytes(i & 0xFF for i in range(n))

    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")
    resp = send_cmd(ser, cmd)
    ser.close()
    assert resp.startswith(b"ok\n\r"), f"Expected ok, got {resp[:20]!r}"
    data = resp[4:]
    assert data == expected, f"Expected {len(expected)} bytes, got {len(data)}"


def test_long_line_truncation():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"> ")

    ser.reset_input_buffer()
    cmd = b"a" * 66
    ser.write(cmd + b"\n")
    resp = ser.read_until(b"> ")
    ser.close()

    line_echo = b"a" * 63 + b"\n\r"
    assert resp.startswith(line_echo), (
        f"Expected 62-char echo, got prefix {resp[:70]!r}"
    )
    assert b"error: unknown command" in resp
