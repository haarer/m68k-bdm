# Hardware-in-the-Loop (HIL) Test Plan

## Overview

Validates the m68k-bdm bridge firmware against a real BDM target simulator on separate hardware.

## Architecture

```
┌─────────────┐   USART1 (115200)   ┌──────────────────┐   BDM wires   ┌──────────────────┐
│   Host PC   │◄──── FTDI (USB) ───►│  Blackpill #1    │◄─────────────►│  Blackpill #2    │
│  (test      │   PB6 TX / PB7 RX   │  BRIDGE (DUT)    │  DSCLK/DSI/   │  TARGET SIM      │
│   harness)  │   /dev/ttyUSB0      │  m68k-bdm fw     │  DSO/FREEZE/  │  cpu32_bdm_sim   │
│             │                     │                  │  RESET        │  firmware        │
└─────────────┘                     └──────────────────┘               └──────────────────┘
```

### Components

| Component | Role | Hardware | Firmware |
|-----------|------|----------|----------|
| **Host PC** | Test harness, command injection, response validation | Any | `tools/tests/test_hil.py` |
| **Blackpill #1** | Device Under Test (DUT) — BDM bridge | STM32F411CEU6 | `bridge/main.elf` |
| **Blackpill #2** | CPU32 target simulator | STM32F411CEU6 | `target_sim/main.elf` |

### Physical Wiring

Both boards are STM32F411 Black Pill, 3.3V — no level shifting. Connect with jumper wires:

| Signal | GPIO | Pin | Direction |
|--------|------|-----|-----------|
| DSCLK (BDM clock) | PA0 | Blackpill pin 10 | Bridge → Target |
| DSI (data in) | PA1 | Blackpill pin 11 | Bridge → Target |
| TARGET_RESET | PA2 | Blackpill pin 12 | Bridge → Target |
| DSO (data out) | PB0 | Blackpill pin 20 | Target → Bridge |
| FREEZE (BDM mode) | PB1 | Blackpill pin 21 | Target → Bridge |
| Common ground | GND | Any GND pin | — |

Each Blackpill also has an FTDI adapter on PB6 (TX) / PB7 (RX) for serial debug output.

## Test Suite

`tools/tests/test_hil.py` exercises every BDM command through the bridge:

| Test Category | Tests | Description |
|---------------|-------|-------------|
| BDM Entry | 2 | Enable sequence, FREEZE assertion |
| Register Access | 4 | Read/write data regs, address regs |
| System Registers | 4 | Read/write SR, PC, VBR, USP/SSP |
| Memory Access | 6 | Read/write byte/word/long, auto-increment |
| Bulk Operations | 4 | DUMP, FILL with various sizes |
| Control Commands | 4 | GO, CALL, RST, NOP |
| Error Handling | 3 | BERR on invalid access, timeout, illegal opcode |
| Protocol Edge Cases | 3 | Rapid commands, status polling, preamble retry |

## Build & Test

```bash
# Build both
make

# Flash bridge (Blackpill #1)
make -C bridge flash

# Flash target simulator (Blackpill #2)
make -C target_sim flash

# Run HIL tests
uv run --directory bridge/test python -m pytest -v test_hil.py
```

## Directory Structure (Current)

```
m68k-bdm/
├── bridge/                  # BDM bridge firmware (DUT)
│   ├── main.c              # Entry point (LED blink, CLI, UART)
│   ├── cli.c/h             # Command-line interface
│   ├── Makefile             # Build system
│   ├── test/                # Loopback tests (via serial)
│   └── SPEC.md              # Bridge BDM spec
├── target_sim/              # CPU32 BDM target simulator
│   ├── main.c               # Entry point (bridge-style)
│   ├── sim_core.c/h         # BDM slave state machine + register file
│   ├── sim_bdm.c/h          # Low-level 17-bit word shift (direct regs)
│   ├── sim_debug.c/h        # Non-blocking UART debug logger
│   ├── board_config.h       # Pin mapping (matches bridge)
│   ├── Makefile             # Build system
│   └── SPEC.md              # Target-side BDM specification
├── common/                  # Shared platform code
│   ├── startup.c            # CMSIS startup (vector table, Reset_Handler)
│   ├── delay.c/h            # SysTick-based delay
│   ├── uart.c/h             # Interrupt-driven UART (USART1, PB6/PB7)
│   ├── syscall.c            # Newlib syscall stubs
│   ├── ringbuf.h            # Inline ring buffer
│   ├── stm32f411.ld         # Linker script
│   └── bdm_defs.h           # BDM opcodes, protocol constants
├── tools/                   # Host-side tools
│   ├── bdm_cli.py           # Python BDM CLI
│   └── tests/               # Legacy tests
├── STM32CubeF4/             # CMSIS headers (third-party)
├── docs/
│   └── HIL_TEST_PLAN.md     # This document
└── Makefile                 # Top-level: all, clean, flash-*
```
