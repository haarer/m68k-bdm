# CPU32 BDM Target-Side Specification

## Overview

This document describes the **target-side** behavior of the Motorola CPU32 Background Debug Mode (BDM) interface. It specifies what the CPU does when a debugger (bridge) communicates with it via the BDM port.

**Reference**: Motorola MC68331 User's Manual, CPU32 Reference Manual, §7.2 "Background Debug Mode"

## Signal Lines

| Signal | Direction | Description |
|--------|-----------|-------------|
| **DSCLK** (BKPT) | Debugger → Target | Serial clock. Debugger drives, target samples. |
| **DSI** (IFETCH) | Debugger → Target | Serial data in. Debugger drives, target samples. |
| **DSO** (IPIPE) | Target → Debugger | Serial data out. Target drives, debugger samples. |
| **FREEZE** | Target → Debugger | Asserted (low) when CPU has entered BDM mode. |
| **RESET** | Debugger → Target | Active-low reset. Debugger toggles to enter BDM. |

## BDM Entry Sequence (§7.2.1)

The CPU enters BDM mode when:

1. **BKPT (DSCLK) is sampled low** at the **rising edge of RESET**
2. BKPT must be held low for **≥2 target clock cycles** before RESET rises
3. Upon entry, the CPU:
   - Asserts FREEZE (drives FREEZE line low)
   - Writes the **BDM entry source code** to ATEMP register
   - Halts execution, saves PC to RPC

### Entry Source Codes (written to ATEMP)

| Source | ATEMP Value |
|--------|-------------|
| Double Bus Fault | `0x00000001` |
| External BKPT (our case) | `0x00000002` |
| BGND Instruction | `0x00000003` |
| Peripheral Breakpoint | `0x00000004` |

### Target Behavior During Entry

```
RESET:  ────╲___________╱────────
                ↑ BKPT sampled here
BKPT:   ────────╲_______________
                  (must be low)
FREEZE: ────────────────────╲___
                              (asserted low)
```

## 17-Bit Word Transfer (§7.2.7)

### Protocol

- **17 bits per word**: 16 data bits + 1 status bit (bit 16)
- **MSB first**
- **Data transitions on falling edge** of DSCLK
- **Data is stable and latched on rising edge** of DSCLK
- Target samples DSI on rising edge, drives DSO on rising edge

### Bit Timing

```
DSCLK:  ────╲___╱───╲___╱───╲___╱───
                 ↑        ↑
            DSI sampled  DSO driven
            by target    by target

DSI:    ────╳═══════════╳═══════════  (data from debugger)
                 ↑
            Target samples here

DSO:    ────╳═══════════╳═══════════  (data from target)
                         ↑
                    Target drives here
```

### Status Bit (Bit 16)

- **Bit 16 = 0**: Target is READY, data word is valid
- **Bit 16 = 1**: Target is NOT READY (busy processing previous command)

When the target is NOT READY:
- The target drives DSO high (bit 16 = 1)
- The accompanying 16-bit data word is **undefined**
- The debugger must continue clocking until bit 16 goes low
- When bit 16 goes low, the accompanying 16-bit word is the **status word**

### Status Word Values

When bit 16 transitions from 1→0, the 16-bit data word is the status:

| Status Word | Meaning |
|-------------|---------|
| `0xFFFF` | OK — command completed successfully |
| `0x8001` | Bus Error — memory access failed |
| `0x0001` | Illegal — invalid opcode or register |
| `0x0000` | Not Ready — still processing (should not occur with bit16=0) |

## Command Processing

After the preamble (BKPT drop → FREEZE assertion), the debugger sends commands as sequences of 17-bit words.

### Command Flow

```
1. Debugger sends preamble (drop BKPT, wait FREEZE)
2. Debugger sends opcode word (17 bits)
3. Target processes opcode:
   a. If command needs address → debugger sends address words
   b. If command needs data → debugger sends data words
   c. Target drives DSO bit 16 = 1 (NOT READY) during processing
4. Target completes processing:
   a. Drives DSO bit 16 = 0 (READY)
   b. Accompanying 16-bit word = status word
5. If command produces result data:
   a. Target sends result words (17 bits each, bit 16 = 0)
```

### Opcode Decoding

Each opcode is a 16-bit value. The target decodes the opcode and executes the corresponding operation.

#### No Operation (NOP)
- **Opcode**: `0x0000`
- **Behavior**: No operation. Returns status `0xFFFF` (OK).
- **Words**: 1 (opcode only)

#### Reset (RST)
- **Opcode**: `0x0100`
- **Behavior**: Reset target peripherals. Returns status `0xFFFF` (OK).
- **Words**: 1 (opcode only)

#### Call (CALL)
- **Opcode**: `0x0200`
- **Behavior**: Call user code at specified address.
- **Additional words**: 32-bit address (MSW first)
- **Returns**: Status `0xFFFF` (OK)
- **Words**: 3 total (opcode + 2 address words)

#### Go (GO)
- **Opcode**: `0x0300`
- **Behavior**: Resume execution from RPC.
- **Words**: 1 (opcode only)
- **Returns**: Status `0xFFFF` (OK)

#### Read A/D Register (RAREG)
- **Opcode**: `0x4200 | (class << 2) | (reg & 0x03)`
- **class**: `0` = data register, `1` = address register
- **reg**: register number (0-7)
- **Returns**: 32-bit register value (MSW first), then status
- **Words**: 4 total (opcode + 2 data words + 1 status word)

#### Write A/D Register (WAREG)
- **Opcode**: `0x4100 | (class << 2) | (reg & 0x03)`
- **Additional words**: 32-bit value (MSW first)
- **Returns**: Status `0xFFFF` (OK)
- **Words**: 4 total (opcode + 2 data words + 1 status word)

#### Read System Register (RSREG)
- **Opcode**: `0x2500 | (select << 3)`
- **select**: system register select code (see table below)
- **Returns**: 32-bit register value (MSW first), then status
- **Words**: 4 total (opcode + 2 data words + 1 status word)

#### Write System Register (WSREG)
- **Opcode**: `0x2400 | (select << 3)`
- **Additional words**: 32-bit value (MSW first)
- **Returns**: Status `0xFFFF` (OK)
- **Words**: 4 total (opcode + 2 data words + 1 status word)

#### Read Memory (READ)
- **Opcode**: `0x0B00 | (size << 2)`
- **size**: `0` = byte, `1` = word, `2` = long
- **Additional words**: 32-bit address (MSW first)
- **Returns**: Data (size-dependent), then status
- **Words**: 3 + data words + 1 status word

#### Write Memory (WRITE)
- **Opcode**: `0x0C00 | (size << 2)`
- **size**: `0` = byte, `1` = word, `2` = long
- **Additional words**: 32-bit address (MSW first) + data
- **Returns**: Status `0xFFFF` (OK) or `0x8001` (bus error)
- **Words**: 3 + data words + 1 status word

#### Dump Memory (DUMP)
- **Opcode**: `0x0F00 | (size << 2)`
- **Behavior**: Read from auto-incrementing address pointer (set by previous READ)
- **Returns**: Data (size-dependent), then status
- **Words**: 1 + data words + 1 status word

#### Fill Memory (FILL)
- **Opcode**: `0x0E00 | (size << 2)`
- **Behavior**: Write to auto-incrementing address pointer (set by previous WRITE)
- **Additional words**: data
- **Returns**: Status `0xFFFF` (OK) or `0x8001` (bus error)
- **Words**: 1 + data words + 1 status word

### System Register Select Codes

| Select Code | Register | Width |
|-------------|----------|-------|
| `0000` (0x00) | RPC (Return PC) | 32-bit |
| `0001` (0x08) | PCC (Current PC) | 32-bit |
| `0010` (0x10) | ATEMP | 32-bit |
| `0011` (0x18) | FAR (Fault Address) | 32-bit |
| `0100` (0x20) | VBR (Vector Base) | 32-bit |
| `0101` (0x28) | SR (Status Register) | 16-bit (upper 16 bits of 32-bit value) |
| `0110` (0x30) | USP (User Stack Ptr) | 32-bit |
| `0111` (0x38) | SSP (Supervisor Stack Ptr) | 32-bit |
| `1000` (0x40) | SFC (Source Function Code) | 3-bit (in lower bits) |
| `1001` (0x48) | DFC (Destination Function Code) | 3-bit (in lower bits) |

### Memory Access Details

#### Address Auto-Increment

After a READ or WRITE operation, the target maintains an internal address pointer that auto-increments:

- **Byte access**: +1
- **Word access**: +2
- **Long access**: +4

Subsequent DUMP/FILL operations use this pointer.

#### Memory Map

The target simulator implements a flat 64KB memory space (0x0000–0xFFFF). All addresses are valid. No bus errors for normal accesses.

Bus errors (`0x8001`) are returned for:
- Addresses outside the valid memory map (if implemented)
- Misaligned word/long accesses (optional, per spec)

## Preamble (§7.2.3)

Before each command, the debugger sends a preamble:

1. Debugger drives BKPT (DSCLK) low
2. Target detects BKPT low → asserts FREEZE (drives FREEZE low)
3. Target is now ready to receive the command opcode
4. After FREEZE assertion, the debugger clocks the opcode word

### Target Preamble Behavior

```
DSCLK:  ────────────╲_______________________
                       (debugger drops BKPT)
FREEZE: ────────────────────╲_______________
                              (target asserts)
```

The target must:
- Detect DSCLK going low
- Assert FREEZE within a few target clock cycles
- Keep FREEZE asserted until the command is complete
- Deassert FREEZE when the command completes (or on GO/CALL)

## Register File

The target maintains the following CPU32 registers:

### Data Registers (D0-D7)
- 32-bit each
- Reset value: undefined (simulator may initialize to 0)

### Address Registers (A0-A7)
- 32-bit each
- A7 is the supervisor stack pointer in normal operation
- Reset value: undefined (simulator may initialize to 0)

### System Registers

| Register | Width | Reset Value | Notes |
|----------|-------|-------------|-------|
| RPC | 32-bit | 0x00000000 | Return PC (saved on BDM entry) |
| PCC | 32-bit | 0x00000000 | Current PC |
| ATEMP | 32-bit | 0x00000002 | BDM entry source (set on entry) |
| FAR | 32-bit | 0x00000000 | Fault address register |
| VBR | 32-bit | 0x00000000 | Vector base register |
| SR | 16-bit | 0x2700 | Status register (supervisor, ints masked) |
| USP | 32-bit | 0x00000000 | User stack pointer |
| SSP | 32-bit | 0x00000000 | Supervisor stack pointer |
| SFC | 3-bit | 0 | Source function code |
| DFC | 3-bit | 0 | Destination function code |

## Memory

The target simulator implements:

- **64KB RAM**: addresses 0x0000–0xFFFF
- **Byte access**: reads/writes single byte
- **Word access**: reads/writes 16-bit value (big-endian)
- **Long access**: reads/writes 32-bit value (big-endian)

### Endianness

CPU32 is **big-endian**:
- Word at address N: byte[N] = MSB, byte[N+1] = LSB
- Long at address N: byte[N] = MSB ... byte[N+3] = LSB

## Error Conditions

| Condition | Status Word | Description |
|-----------|-------------|-------------|
| Invalid opcode | `0x0001` | Opcode not recognized |
| Invalid register | `0x0001` | Register number out of range |
| Bus error | `0x8001` | Memory access failed |
| OK | `0xFFFF` | Command completed successfully |

## Timing Requirements

- **BDM clock**: 500 kHz typical (1 µs half-period)
- **DSCLK → DSO delay**: Target must drive DSO within 100 ns of DSCLK rising edge
- **FREEZE assertion**: Within 10 target clock cycles of BKPT drop
- **Command processing**: Variable; target drives NOT READY (bit 16 = 1) while processing

## Implementation Notes

1. **DSO is tri-state**: When not driving data, the target should drive DSO high (pull-up). This is the NOT READY state.
2. **FREEZE is active-low**: Asserted = low, deasserted = high.
3. **RESET is active-low**: Asserted = low, deasserted = high.
4. **BDM entry is one-time**: Once entered, the target stays in BDM mode until GO or CALL releases it.
5. **GO/CALL exits BDM**: These commands cause the target to deassert FREEZE and resume execution.
