# Hardware-in-the-Loop (HIL) Test Plan

## Overview

This document describes the HIL test architecture for validating the m68k-bdm bridge firmware against a real BDM target simulator running on separate hardware.

## Architecture

```
┌─────────────┐   USB CDC    ┌──────────────────┐   BDM wires   ┌──────────────────┐
│   Host PC   │◄────────────►│  Blackpill #1    │◄─────────────►│  Blackpill #2    │
│  (test      │              │  BRIDGE (DUT)    │  DSCLK/DSI/   │  TARGET SIM      │
│   harness)  │              │  m68k-bdm fw     │  DSO/FREEZE/  │  cpu32_bdm_sim   │
│             │              │                  │  RESET        │  firmware        │
└─────────────┘              └──────────────────┘               └──────────────────┘
```

### Components

| Component | Role | Hardware | Firmware |
|-----------|------|----------|----------|
| **Host PC** | Test harness, command injection, response validation | Any | `tests/test_hil.py` |
| **Blackpill #1** | Device Under Test (DUT) — BDM bridge | STM32F103C8T6 | `m68k-bdm` (VARIANT=stm32f1) |
| **Blackpill #2** | CPU32 target simulator | STM32F103C8T6 | `target_sim` (VARIANT=target_sim) |

### Physical Wiring

Both boards use the same pin mapping. Connect corresponding pins directly with jumper wires.
Refer to the pinout diagram in `docs/Pinout-Diagram.png`.

| Signal | GPIO | Bridge Board Pin | Target Sim Board Pin | Direction |
|--------|------|-----------------|---------------------|-----------|
| DSCLK (BDM clock) | PA0 | Right header, pin 7 | Right header, pin 7 | Bridge → Target |
| DSI (data in to target) | PA1 | Right header, pin 8 | Right header, pin 8 | Bridge → Target |
| DSO (data out from target) | PA2 | Right header, pin 9 | Right header, pin 9 | Target → Bridge |
| FREEZE (BDM mode indicator) | PA3 | Right header, pin 10 | Right header, pin 10 | Target → Bridge |
| TARGET_RESET | PA4 | Right header, pin 11 | Right header, pin 11 | Bridge → Target |
| Common ground | GND | Right header, pin 2 | Right header, pin 2 | — |

Both boards are 3.3V — no level shifting required.

## Development Methodology: Independent Implementation

### Principle

Two engineers implement the BDM protocol **independently**, each reading only the CPU32 specification. Neither sees the other's code. Mismatches during HIL testing reveal spec ambiguities or implementation bugs.

### Session Isolation

```
Session 1: Bridge Engineer
├── Sees: CPU32 BDM debugger-side spec
├── Sees: existing bridge code (common/, arch/)
├── Sees: HAL API (hal/hal.h)
├── Sees: HIL test harness (tests/test_hil.py)
└── CANNOT see: target_sim/

Session 2: Target Simulator Engineer
├── Sees: CPU32 BDM target-side spec (target_sim/SPEC.md)
├── Sees: HAL API (hal/hal.h)
├── Sees: board_config.h for target_sim
└── CANNOT see: common/, arch/, tests/

Session 3: Integration
├── Both directories visible
├── Wire boards together
├── Run HIL tests
└── Debug mismatches → spec ambiguities surface
```

### Why This Works

1. **Independent implementations catch spec ambiguities** — if both engineers interpret "DSO bit 16 = 0 means ready" differently, the test fails and the ambiguity is exposed
2. **No mock dependencies** — the target simulator runs real BDM timing on real hardware
3. **Test harness is the arbiter** — it validates against the spec, not against either implementation
4. **Reusable** — once the target sim exists, any bridge port (AVR, STM32, future) can be tested against it

## Target Simulator Specification

The target simulator implements the **CPU32 BDM slave** — what the MC68331 does when a debugger connects. See `target_sim/SPEC.md` for the complete target-side specification.

Key behaviors:
- **17-bit word receiver**: Clocks in on DSCLK rising edge, samples DSI, drives DSO
- **Opcode decoder**: RAREG, WAREG, RSREG, WSREG, READ, WRITE, DUMP, FILL, GO, CALL, RST, NOP
- **Register file**: D0-D7, A0-A7, SR, PC, USP, SSP, SFC, DFC, VBR, FAR, ATEMP
- **Memory**: 64KB RAM array, byte/word/long access with address auto-increment
- **Status generation**: READY/NOT_READY/BERR/ILLEGAL per spec
- **BDM entry**: FREEZE assertion when BKPT sampled low at RESET rising edge

## Test Suite

The HIL test suite (`tests/test_hil.py`) exercises every BDM command through the bridge:

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

## Build Instructions

```bash
# Build bridge firmware (Blackpill #1)
make VARIANT=stm32f1 all
st-flash write build/stm32f1/bdm_bridge.bin 0x08000000

# Build target simulator (Blackpill #2)
make VARIANT=target_sim all
st-flash write build/target_sim/bdm_target_sim.bin 0x08000000

# Run HIL tests
make VARIANT=stm32f1 test-hil PORT=/dev/ttyACM0 TARGET_PORT=/dev/ttyACM1
```

## Directory Structure

```
m68k-bdm/
├── common/                  # Bridge firmware (shared)
├── arch/                    # HAL implementations
│   ├── avr/
│   └── stm32f1/
├── target_sim/              # CPU32 BDM target simulator
│   ├── SPEC.md              # Target-side BDM specification
│   ├── sim_core.c/h         # BDM slave state machine
│   ├── sim_bdm.c/h          # Low-level 17-bit word shift (slave)
│   └── main.c               # Entry point
├── tests/
│   ├── test_monitor.py      # Protocol framing tests
│   └── test_hil.py          # Hardware-in-the-loop tests
├── docs/
│   └── HIL_TEST_PLAN.md     # This document
└── Makefile                 # VARIANT=avr|stm32f1|target_sim
```
