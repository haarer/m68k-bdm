#!/usr/bin/env python3
"""
bdm_cli.py  –  Interactive BDM command-line tool for the bridge.

Usage:
    python3 bdm_cli.py [-p PORT] [-b BAUD]

Connects to the flashed bridge and provides a readline-based REPL
for sending BDM commands to the CPU32 target.

Type 'help' at the prompt for available commands.
"""

import argparse
import readline
import struct
import sys
import time

# ------------------------------------------------------------------
#  Protocol constants (must match config.h)
# ------------------------------------------------------------------

STX = 0x02
ETX = 0x03

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

RSP_OK            = 0x00
RSP_ERROR         = 0x01
RSP_NOT_SUPPORTED = 0x02
RSP_TIMEOUT       = 0x03
RSP_TARGET_ERROR  = 0x04

RSP_NAMES = {
    RSP_OK:            "OK",
    RSP_ERROR:         "ERROR",
    RSP_NOT_SUPPORTED: "NOT_SUPPORTED",
    RSP_TIMEOUT:       "TIMEOUT",
    RSP_TARGET_ERROR:  "TARGET_ERROR",
}

DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUD = 115200

# ------------------------------------------------------------------
#  Frame builder
# ------------------------------------------------------------------

def build_frame(cmd, payload=b""):
    cs = STX ^ cmd ^ len(payload)
    for b in payload:
        cs ^= b
    return bytes([STX, cmd, len(payload)]) + payload + bytes([cs, ETX])


# ------------------------------------------------------------------
#  Bridge client
# ------------------------------------------------------------------

class Bridge:
    def __init__(self, port, baud):
        import serial
        self.port = serial.Serial(port, baud, timeout=2, dsrdtr=True)
        time.sleep(2)
        self.port.flushInput()
        self.port.flushOutput()

    def close(self):
        self.port.close()

    def _read_response(self):
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
        if raw[0] != STX or not (raw[1] & 0x80):
            return None, None, bytes(raw)
        rsp_code = raw[1] & 0x7F
        rsp_len = raw[2]
        rsp_payload = bytes(raw[3:3 + rsp_len])
        cs_byte = raw[3 + rsp_len]
        if raw[3 + rsp_len + 1] != ETX:
            return None, None, bytes(raw)
        expected_cs = STX ^ raw[1] ^ raw[2]
        for b in rsp_payload:
            expected_cs ^= b
        if expected_cs != cs_byte:
            return None, None, bytes(raw)
        return rsp_code, rsp_payload, bytes(raw)

    def send(self, cmd, payload=b""):
        self.port.flushInput()
        self.port.write(build_frame(cmd, payload))
        time.sleep(0.15)
        return self._read_response()[:2]

    def send_raw(self, cmd_byte, payload=b""):
        self.port.flushInput()
        self.port.write(build_frame(cmd_byte, payload))
        time.sleep(0.15)
        return self._read_response()[:2]


# ------------------------------------------------------------------
#  Helpers
# ------------------------------------------------------------------

def fmt_addr(addr):
    return f"0x{addr:08X}"

def u32_to_hex(val):
    return f"0x{val:08X}"

def rsp_name(code):
    return RSP_NAMES.get(code, f"UNKNOWN(0x{code:02X})")

def parse_hex(s):
    if s.startswith("0x") or s.startswith("0X"):
        return int(s, 16)
    if s.startswith("$"):
        return int(s[1:], 16)
    return int(s, 0)

def parse_size(s):
    val = parse_hex(s)
    if val in (1, 2, 4):
        return val
    return 1

def payload_to_u32(payload, offset=0):
    return struct.unpack(">I", payload[offset:offset+4])[0]


# ------------------------------------------------------------------
#  Command dispatch
# ------------------------------------------------------------------

COMMANDS = {
    "enable":       ("BDM Enable (§7.2.1 reset sequence)",              0),
    "status":       ("Query bridge/target status",                      0),
    "config":       ("Configure bridge parameters",                     0),
    "halt":        ("Halt target execution",                             0),
    "go":          ("Resume target execution",                           0),
    "reset":       ("Reset target",                                      0),
    "step":        ("Single-step target",                                0),
    "mread":       ("Read memory",                                       2),
    "mwrite":      ("Write memory",                                      2),
    "mdump":       ("Dump memory block",                                 2),
    "mfill":       ("Fill memory block",                                 2),
    "regread":     ("Read data/address register",                        1),
    "regwrite":    ("Write data/address register",                       2),
    "sysregread":  ("Read system register",                              1),
    "sysregwrite": ("Write system register",                             2),
    "call":        ("Call target code at address",                       1),
    "bpset":       ("Set hardware breakpoint",                           1),
    "bpclr":       ("Clear hardware breakpoint",                         1),
}

HELP_TEXT = """\
BDM CLI – Interactive command tool for CPU32 bridge

Commands:
  enable               Enable BDM on target (§7.2.1 reset sequence)
  status               Query bridge and target status
  config               Configure bridge parameters

  halt                 Halt target execution
  go                   Resume target execution
  reset                Reset target
  step                 Single-step target execution

  mread  ADDR [COUNT] [SIZE]   Read memory (SIZE: 1=byte, 2=word, 4=long)
  mwrite ADDR DATA...          Write memory (DATA as hex bytes/words/longs)
  mdump  ADDR [COUNT] [SIZE]   Dump memory block (auto-increment)
  mfill  ADDR COUNT VALUE [SZ] Fill memory block with VALUE

  regread  REG               Read data register (0-7) or address register (8-15)
  regwrite REG VALUE         Write data/address register

  sysregread  SEL            Read system register (SEL: 0=RPC,1=PCC,3=SR,6=USP,7=SSP,8=SFC,9=DFC,2=ATEMP,3=FAR,4=VBR)
  sysregwrite SEL VALUE      Write system register

  call ADDR                  Call target code at address
  bpset  ADDR                Set hardware breakpoint
  bpclr  ADDR                Clear hardware breakpoint

  raw CMD [DATA...]          Send raw command byte with optional hex data
  help                       Show this help
  quit / exit / ^D           Disconnect and exit

Notes:
  - Addresses and values accept hex (0x prefix), $ prefix, or plain decimal.
  - mwrite accepts comma-separated hex bytes (e.g. mwrite 0x1000 0xAB,0xCD,0xEF).
"""

SYSREG_NAMES = {
    0: "RPC", 1: "PCC", 2: "ATEMP", 3: "SR", 3: "FAR",
    4: "VBR", 6: "USP", 7: "SSP", 8: "SFC", 9: "DFC",
}


def cmd_enable(br, args):
    code, payload = br.send(CMD_BDM_ENABLE)
    if code is None:
        return "ERROR: no response from bridge"
    ok = payload[0] if payload else 0
    status = "enabled" if ok else "failed"
    return f"BDM enable {status} [{rsp_name(code)}]"


def cmd_status(br, args):
    code, payload = br.send(CMD_STATUS)
    if code is None:
        return "ERROR: no response from bridge"
    mode = "BDM" if (payload and payload[0]) else "normal"
    return f"Target in {mode} mode [{rsp_name(code)}]"


def cmd_config(br, args):
    code, _ = br.send(CMD_CONFIG)
    return f"Config OK [{rsp_name(code)}]" if code is not None else "ERROR: no response"


def cmd_halt(br, args):
    code, _ = br.send(CMD_TARGET_HALT)
    return f"Halt [{rsp_name(code)}]"


def cmd_go(br, args):
    code, _ = br.send(CMD_TARGET_GO)
    return f"Go [{rsp_name(code)}]"


def cmd_reset(br, args):
    code, _ = br.send(CMD_TARGET_RESET)
    return f"Reset [{rsp_name(code)}]"


def cmd_step(br, args):
    code, _ = br.send(CMD_STEP)
    return f"Step [{rsp_name(code)}]"


def cmd_mread(br, args):
    if not args:
        return "Usage: mread ADDR [COUNT] [SIZE]"
    addr = parse_hex(args[0])
    count = parse_hex(args[1]) if len(args) > 1 else 1
    size = parse_size(args[2]) if len(args) > 2 else 1
    code, payload = br.send(CMD_MEM_READ, struct.pack(">I", addr) + bytes([count, size]))
    if code is None:
        return "ERROR: no response"
    if code != RSP_OK:
        return f"mread {fmt_addr(addr)} [{rsp_name(code)}]"
    if not payload:
        return f"mread {fmt_addr(addr)}: (empty) [OK]"
    val = payload_to_u32(payload)
    return f"mread {fmt_addr(addr)} = {u32_to_hex(val)} [OK]"


def cmd_mwrite(br, args):
    if len(args) < 2:
        return "Usage: mwrite ADDR DATA..."
    addr = parse_hex(args[0])
    data = []
    for s in args[1:]:
        for part in s.split(","):
            part = part.strip()
            if part:
                data.append(parse_hex(part) & 0xFF)
    code, _ = br.send(CMD_MEM_WRITE, struct.pack(">I", addr) + bytes(data))
    if code is None:
        return "ERROR: no response"
    return f"mwrite {fmt_addr(addr)} {len(data)} bytes [{rsp_name(code)}]"


def cmd_mdump(br, args):
    if not args:
        return "Usage: mdump ADDR [COUNT] [SIZE]"
    addr = parse_hex(args[0])
    count = parse_hex(args[1]) if len(args) > 1 else 8
    size = parse_size(args[2]) if len(args) > 2 else 4
    code, payload = br.send(CMD_MEM_DUMP, struct.pack(">I", addr) + bytes([count, size]))
    if code is None:
        return "ERROR: no response"
    if code != RSP_OK:
        return f"mdump {fmt_addr(addr)} [{rsp_name(code)}]"
    lines = [f"mdump {fmt_addr(addr)} ({count}x{'BWL'[size-1] if size <= 4 else '?'}) [OK]:"]
    if payload:
        for i in range(0, len(payload), 4):
            chunk = payload[i:i+4]
            val = struct.unpack(">I", chunk + b'\x00' * (4 - len(chunk)))[0]
            lines.append(f"  {fmt_addr(addr + i)}  {u32_to_hex(val)}")
    return "\n".join(lines)


def cmd_mfill(br, args):
    if len(args) < 3:
        return "Usage: mfill ADDR COUNT VALUE [SIZE]"
    addr = parse_hex(args[0])
    count = parse_hex(args[1])
    value = parse_hex(args[2])
    size = parse_size(args[3]) if len(args) > 3 else 1
    code, _ = br.send(CMD_MEM_FILL, struct.pack(">I", addr) + bytes([count]) + struct.pack(">I", value) + bytes([size]))
    if code is None:
        return "ERROR: no response"
    return f"mfill {fmt_addr(addr)} {count}x {u32_to_hex(value)} [{rsp_name(code)}]"


def cmd_regread(br, args):
    if not args:
        return "Usage: regread REG (0-7=D, 8-15=A)"
    reg = parse_hex(args[0])
    code, payload = br.send(CMD_REG_READ, bytes([reg]))
    if code is None:
        return "ERROR: no response"
    if code != RSP_OK:
        return f"regread {reg} [{rsp_name(code)}]"
    val = payload_to_u32(payload) if payload else 0
    name = f"D{reg}" if reg < 8 else f"A{reg-8}"
    return f"{name} = {u32_to_hex(val)} [OK]"


def cmd_regwrite(br, args):
    if len(args) < 2:
        return "Usage: regwrite REG VALUE"
    reg = parse_hex(args[0])
    value = parse_hex(args[1])
    code, _ = br.send(CMD_REG_WRITE, bytes([reg]) + struct.pack(">I", value))
    if code is None:
        return "ERROR: no response"
    name = f"D{reg}" if reg < 8 else f"A{reg-8}"
    return f"{name} <- {u32_to_hex(value)} [{rsp_name(code)}]"


def cmd_sysregread(br, args):
    if not args:
        return "Usage: sysregread SEL"
    sel = parse_hex(args[0])
    code, payload = br.send(CMD_SYSREG_READ, bytes([sel]))
    if code is None:
        return "ERROR: no response"
    if code != RSP_OK:
        return f"sysregread {sel} [{rsp_name(code)}]"
    val = payload_to_u32(payload) if payload else 0
    rname = SYSREG_NAMES.get(sel, f"SEL{sel}")
    return f"{rname} = {u32_to_hex(val)} [OK]"


def cmd_sysregwrite(br, args):
    if len(args) < 2:
        return "Usage: sysregwrite SEL VALUE"
    sel = parse_hex(args[0])
    value = parse_hex(args[1])
    code, _ = br.send(CMD_SYSREG_WRITE, bytes([sel]) + struct.pack(">I", value))
    if code is None:
        return "ERROR: no response"
    rname = SYSREG_NAMES.get(sel, f"SEL{sel}")
    return f"{rname} <- {u32_to_hex(value)} [{rsp_name(code)}]"


def cmd_call(br, args):
    if not args:
        return "Usage: call ADDR"
    addr = parse_hex(args[0])
    code, _ = br.send(CMD_CALL, struct.pack(">I", addr))
    if code is None:
        return "ERROR: no response"
    return f"call {fmt_addr(addr)} [{rsp_name(code)}]"


def cmd_bpset(br, args):
    addr = parse_hex(args[0]) if args else 0
    code, _ = br.send(CMD_BREAKPOINT_SET)
    if code is None:
        return "ERROR: no response"
    return f"bpset {fmt_addr(addr)} [{rsp_name(code)}]"


def cmd_bpclr(br, args):
    addr = parse_hex(args[0]) if args else 0
    code, _ = br.send(CMD_BREAKPOINT_CLR)
    if code is None:
        return "ERROR: no response"
    return f"bpclr {fmt_addr(addr)} [{rsp_name(code)}]"


def cmd_raw(br, args):
    if not args:
        return "Usage: raw CMD_BYTE [DATA...]"
    cmd_byte = parse_hex(args[0])
    data = bytes([parse_hex(s) & 0xFF for s in args[1:]])
    code, payload = br.send_raw(cmd_byte, data)
    if code is None:
        return "ERROR: no response"
    lines = [f"raw 0x{cmd_byte:02X} [{rsp_name(code)}]"]
    if payload:
        lines.append(f"  payload: {' '.join(f'0x{b:02X}' for b in payload)}")
    return "\n".join(lines)


HANDLERS = {
    "enable":        cmd_enable,
    "status":        cmd_status,
    "config":        cmd_config,
    "halt":         cmd_halt,
    "go":           cmd_go,
    "reset":        cmd_reset,
    "step":         cmd_step,
    "mread":        cmd_mread,
    "mwrite":       cmd_mwrite,
    "mdump":        cmd_mdump,
    "mfill":        cmd_mfill,
    "regread":      cmd_regread,
    "regwrite":     cmd_regwrite,
    "sysregread":   cmd_sysregread,
    "sysregwrite":  cmd_sysregwrite,
    "call":         cmd_call,
    "bpset":        cmd_bpset,
    "bpclr":        cmd_bpclr,
    "raw":          cmd_raw,
}

# Tab-completion words
COMPLETION_WORDS = list(COMMANDS.keys()) + ["help", "quit", "exit"]


def tab_complete(text, state):
    matches = [w for w in COMPLETION_WORDS if w.startswith(text)]
    try:
        return matches[state]
    except IndexError:
        return None


# ------------------------------------------------------------------
#  REPL
# ------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Interactive BDM CLI for CPU32 bridge")
    ap.add_argument("-p", "--port", default=DEFAULT_PORT, help="Serial port (default: /dev/ttyACM0)")
    ap.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD, help="Baud rate (default: 115200)")
    args = ap.parse_args()

    try:
        br = Bridge(args.port, args.baud)
    except Exception as e:
        print(f"Cannot open serial port {args.port}: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Connected to bridge on {args.port} @ {args.baud} baud")
    print("Type 'help' for available commands. Ctrl-D or 'quit' to exit.\n")

    readline.parse_and_bind("tab: complete")
    readline.set_completer(tab_complete)

    try:
        while True:
            try:
                line = input("bdm> ").strip()
            except EOFError:
                print()
                break
            if not line:
                continue
            parts = line.split()
            cmd = parts[0].lower()
            rest = parts[1:]

            if cmd in ("quit", "exit"):
                break
            if cmd == "help":
                print(HELP_TEXT)
                continue

            handler = HANDLERS.get(cmd)
            if handler is None:
                print(f"Unknown command: {cmd}  (type 'help' for list)")
                continue

            try:
                result = handler(br, rest)
                print(result)
            except Exception as e:
                print(f"ERROR: {e}")
    finally:
        br.close()
        print("Disconnected.")


if __name__ == "__main__":
    main()
