import os
import serial
import subprocess
import time

SERIAL_PORT = os.environ.get("TEST_SERIAL_PORT", "/dev/ttyUSB0")
BAUDRATE = 115200
TIMEOUT = 5
EXPECTED_BOOT = b"BDM bridge ready\n\rType 'help' for commands.\n\rbdm> "


def reset_target():
    subprocess.run(["st-flash", "reset"], capture_output=True)


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


def test_boot_message():
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.reset_input_buffer()
    reset_target()
    data = ser.read_until(b"bdm> ")
    ser.close()
    assert data == EXPECTED_BOOT, f"Expected {EXPECTED_BOOT!r}, got {data!r}"


def test_hello_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"hello")
    ser.close()
    assert resp == b"hello world\n\r", f"Expected hello world, got {resp!r}"


def test_echo_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"echo HELLOCLI")
    ser.close()
    assert resp == b"HELLOCLI\n\r", f"Expected HELLOCLI, got {resp!r}"


def test_led_on_off():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"led on")
    assert resp == b"ok\n\r"
    resp = send_cmd(ser, b"led off")
    assert resp == b"ok\n\r"
    ser.close()


def test_help_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"help")
    ser.close()
    assert b"BDM CLI commands:" in resp
    assert b"enable" in resp
    assert b"status" in resp
    assert b"halt" in resp
    assert b"go" in resp
    assert b"mread" in resp
    assert b"mwrite" in resp
    assert b"regread" in resp
    assert b"regwrite" in resp
    assert b"hello" in resp
    assert b"led on" in resp


def test_unknown_command():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"notacommand")
    ser.close()
    assert b"error: unknown command" in resp


def test_empty_line():
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    ser.reset_input_buffer()
    ser.write(b"\n")
    resp = ser.read_until(b"bdm> ")
    ser.close()
    assert resp == b"\n\rbdm> ", f"Expected echo + prompt, got {resp!r}"


def test_status_no_target():
    """STATUS should return 'normal mode' when no target connected."""
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"status")
    ser.close()
    assert b"normal mode" in resp


def test_enable_no_target():
    """BDM enable should fail gracefully without a target."""
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"enable")
    ser.close()
    assert b"FAIL" in resp or b"error" in resp


def test_nop_no_target():
    """NOP command should be recognized even without a target."""
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    resp = send_cmd(ser, b"nop")
    ser.close()
    assert b"error: unknown command" not in resp


def test_halt_go_reset_no_target():
    """halt/go/reset should be recognized (will fail without target)."""
    reset_target()
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
    ser.read_until(b"bdm> ")
    for cmd in [b"halt", b"go", b"reset", b"step"]:
        ser.reset_input_buffer()
        ser.write(cmd + b"\n")
        resp = ser.read_until(b"bdm> ")
        assert b"error: unknown command" not in resp, f"{cmd} was rejected"
    ser.close()
