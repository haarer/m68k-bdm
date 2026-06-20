import os
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
    time.sleep(3)
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


def test_bdm_enable():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"enable")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_status():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"status")
        assert b"normal mode" in resp or b"BDM mode" in resp
    finally:
        ser.close()


def test_bdm_halt():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"halt")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_go():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"go")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_reset():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"reset")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_step():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"step")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_nop():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"nop")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_mread_byte():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"mread 0x1000 1")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_mread_word():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"mread 0x1000 2")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_mread_long():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"mread 0x1000 4")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_mwrite():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"mwrite 0x1000 0xAABBCCDD")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_regread_data():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"regread 0")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_regread_addr():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"regread 8")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_regwrite():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"regwrite 0 0xDEADBEEF")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_sysreg():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"sysreg 0")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_syswr():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"syswr 0 0x12345678")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_call():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"call 0x2000")
        assert b"error: unknown command" not in resp
    finally:
        ser.close()


def test_bdm_all_commands_help():
    _ensure_reset()
    ser = _open_serial()
    try:
        _reset_and_wait_prompt(ser)
        resp = send_cmd(ser, b"help")
        for cmd in [b"enable", b"status", b"halt", b"go", b"reset", b"step",
                     b"mread", b"mwrite", b"regread", b"regwrite",
                     b"sysreg", b"syswr", b"call", b"nop"]:
            assert cmd in resp, f"help missing {cmd.decode()}"
    finally:
        ser.close()
