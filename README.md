# STM32F411 BDM Debug Bridge for Motorola CPU32 (MC68331)

## Project Overview

This project implements a hardware debug bridge that connects a GNU toolchain on a Linux host to a Motorola CPU32 target (MC68331) via the Background Debug Mode (BDM) protocol. The bridge firmware runs on an STM32F411 Cortex-M4 microcontroller, translating between a serial interface toward the host PC and the BDM bus toward the target. An interactive Python CLI tool (`bdm_cli.py`) provides a command-line interface for sending BDM commands to the bridge.

## Architecture

```
┌──────────────┐        Serial (UART)        ┌─────────────────┐        BDM Bus      ┌──────────────┐
│  Linux Host  │ ◄────────────────────────►  │  STM32F411      │ ◄────────────────►  │  MC68331     │
│              │  Custom command protocol    │  Bridge          │  BDM protocol       │  (Target)    │
│  bdm_cli.py  │                             │  Firmware        │                      │              │
└──────────────┘                             └─────────────────┘                     └──────────────┘
```

## System Components

### 1. STM32F411 Bridge Firmware (`bridge/`)

The firmware running on the STM32F411 microcontroller performs the following functions:

- **Serial Interface (Host Side)**
  - UART communication with the Linux host PC (USART1, PB6/PB7 via FTDI adapter)
  - Parses commands from the host using a custom command-response protocol
  - Returns responses, memory dumps, register states, and status information
  - Configurable baud rate (default 115200 bps)
  - Interrupt-driven with ring buffers for non-blocking I/O

- **BDM Protocol Engine (Target Side)**
  - Implements the CPU32 BDM serial interface per Reference Manual §7.2.7
  - Generates DSCLK (BKPT), drives DSI (IFETCH), reads DSO (IPIPE)
  - Full-duplex 17-bit word shifting (16 data + 1 status bit)
  - Handles BDM command frames: address setup, read/write, register access, control commands
  - Monitors FREEZE signal for BDM entry detection

- **Command Processing**
  - Translates host commands into BDM bus transactions in `main.c`
  - Assembles and disassembles BDM frames with proper bit-banging
  - Handles acknowledgment, error detection, and retry logic

### 2. Host CLI Tool

`bdm_cli.py` is an interactive Python tool that communicates with the bridge over serial:

- **Command Interface** — Readline-based REPL with tab completion and command history
- **All BDM Commands** — Memory read/write/dump/fill, register access, target control
- **Raw Mode** — Send arbitrary command bytes for protocol experimentation
- **Human-Readable Output** — Response codes decoded to names, memory dumps formatted with addresses

### 3. Hardware

- **STM32F411 Microcontroller (Black Pill)**
  - Target MCU: STM32F411CEU6 (Cortex-M4, 100 MHz)
  - 512 KiB flash, 128 KiB SRAM
  - On-board LED on PC13

- **BDM Interface Circuit**
  - Level shifting if target operates at different voltage
  - BDM pins: DSCLK/BKPT (clock, output), DSI/IFETCH (data in, output),
    DSO/IPIPE (data out, input), FREEZE (BDM entry, input)
  - Target reset control line

- **Serial Interface (FTDI Adapter)**
  - External FTDI USB-to-serial adapter connected to USART1 (PB6 TX, PB7 RX)
  - Appears as `/dev/ttyUSB0` on Linux
  - Configurable baud rate (default 115200)
  - Power delivery via USB from either the FTDI or the ST-Link

## BDM Protocol Specification

The BDM protocol for CPU32 uses a full-duplex synchronous serial interface per
the CPU32 Reference Manual §7.2.7. The development system (bridge) is the master.

### Signal Lines (per CPU32 §7.2.7)
- **DSCLK** (BKPT pin): Serial clock, output from bridge. Data transitions on
  falling edge, latched on rising edge. Remains high between transfers.
- **DSI** (IFETCH pin): Serial data input to CPU, output from bridge.
- **DSO** (IPIPE pin): Serial data output from CPU, input to bridge. Bit 16 of
  each 17-bit word is the status/control bit: 1 = not ready, 0 = ready.
- **FREEZE**: CPU output indicating BDM entry. Asserted (low) when CPU enters
  BDM, negated when CPU returns to normal mode.

### BDM Enable (CPU32 §7.2.1)
BDM is enabled once during reset, not on every command. The bridge asserts BKPT
(low) while toggling the target RESET line. BKPT must be held low for at least
two target clock cycles before RESET goes high. At the rising edge of RESET,
BKPT samples low and BDM is enabled. BDM remains enabled until the next system
reset. The bridge calls `bdm_enable()` once at startup.

### Command Frame Format
Each BDM transaction consists of:
1. **Preamble**: Drop BKPT (DSCLK low), wait for FREEZE assertion, then begin clocking
2. **17-bit Words**: 16 data bits + 1 status bit on DSO, shifted MSB first,
   full-duplex (DSI and DSO simultaneous)
3. **Address/Parameter Fields**: Variable length depending on command
4. **Data Fields**: Payload for read/write operations
5. **Status Poll**: Bridge polls DSO bit 16 until CPU indicates ready (bit 16 = 0)

### Supported BDM Operations
- Memory read (byte, word, long)
- Memory write (byte, word, long)
- Register read/write (general purpose, special purpose, MMU)
- Program counter read/write
- Target reset (hardware and software)
- Breakpoint set/clear
- Single-step execution
- Go/continue execution
- Halt target

## BDM Command Specification

The CPU32 BDM protocol defines 12 commands, each encoded as a 16-bit operation word. The following table documents each command per the Motorola CPU32 Reference Manual and MC68331 User's Manual, along with the current firmware implementation status.

### Command Registry

| # | Command | Mnemonic | 16-bit Opcode Pattern | Firmware Status |
|---|---------|----------|----------------------|-----------------|
| 1 | Read A/D Register | RAREG/RDREG | `0x4200 \| (A/D<<2) \| REG` | **Implemented** |
| 2 | Write A/D Register | WAREG/WDREG | `0x4100 \| (A/D<<2) \| REG` | **Implemented** |
| 3 | Read System Register | RSREG | `0x2500 \| (REG<<3)` | **Implemented** |
| 4 | Write System Register | WSREG | `0x2400 \| (REG<<3)` | **Implemented** |
| 5 | Read Memory Location | READ | `0x0B00 \| (SIZE<<2)` | **Implemented** |
| 6 | Write Memory Location | WRITE | `0x0C00 \| (SIZE<<2)` | **Implemented** |
| 7 | Dump Memory Block | DUMP | `0x0F00 \| (SIZE<<2)` | **Implemented** |
| 8 | Fill Memory Block | FILL | `0x0E00 \| (SIZE<<2)` | **Implemented** |
| 9 | Resume Execution | GO | `0x0300` | **Implemented** |
| 10 | Call User Code | CALL | `0x0200` | **Implemented** |
| 11 | Reset Peripherals | RST | `0x0100` | **Implemented** |
| 12 | No Operation | NOP | `0x0000` | **Implemented** |

### System Register Select Codes (RSREG/WSREG)

| Register | Select Code |
|----------|------------|
| Return Program Counter (RPC) | `0000` |
| Current Instruction PC (PCC) | `0001` |
| Temporary Register A (ATEMP) | `1000` |
| Fault Address Register (FAR) | `1001` |
| Vector Base Register (VBR) | `1010` |
| Status Register (SR) | `1011` |
| User Stack Pointer (USP) | `1100` |
| Supervisor Stack Pointer (SSP) | `1101` |
| Source Function Code (SFC) | `1110` |
| Destination Function Code (DFC) | `1111` |

### BDM Entry Source Codes (ATEMP)

When entering BDM, the CPU writes a source code to ATEMP. The first command after entering BDM should be RSREG to read ATEMP before it gets overwritten.

| Source | ATEMP Value |
|--------|------------|
| Double Bus Fault | `$00000001` |
| External BKPT | `$00000002` |
| BGND Instruction | `$00000003` |
| Peripheral Breakpoint | `$00000004` |

### What's Implemented

All 12 BDM commands from the CPU32 specification are now implemented:

1. **BDM Enable** - One-time reset sequence per §7.2.1: BKPT asserted low, target RESET toggled so BKPT samples low at rising edge, FREEZE monitored
2. **Preamble** - Per-command BKPT drop + FREEZE wait (separate from BDM enable)
3. **RAREG/WAREG** - Read/write data and address registers (D0-D7, A0-A7)
4. **RSREG/WSREG** - Read/write system registers (RPC, PCC, SR, USP, SSP, SFC, DFC, ATEMP, FAR, VBR)
5. **READ/WRITE** - Memory access with byte, word, and long size support
6. **DUMP/FILL** - Bulk memory transfers with auto-incrementing address pointer
7. **GO/CALL** - Resume execution or call user code at specified address
8. **RST/NOP** - Target reset and no-op for inter-command padding
9. **STEP** - Single-step execution via PC inspection and instruction length calculation
10. **16-bit word shifting** - BDM protocol uses 16-bit words shifted MSB-first on BDD

### Remaining Work

1. **Breakpoint hardware** - `CMD_BREAKPOINT_SET`/`CMD_BREAKPOINT_CLR` return OK but don't use hardware breakpoint support
2. **Error recovery** - BERR/AERR detection works, but retry logic is minimal
3. **MC68331-specific extensions** - Target-specific register map and peripheral debug not yet implemented
4. **Instruction length decoding** - `bdm_step()` has simplified instruction length calculation

## Host Communication Protocol

### Serial Protocol
The bridge speaks a simple command-response protocol over UART:

```
Command format:  [STX][CMD][LEN][PAYLOAD...][CHECKSUM][ETX]
Response format: [STX][RSP][LEN][PAYLOAD...][CHECKSUM][ETX]
```

### Command Set
| Code | Command         | Description                        |
|------|-----------------|------------------------------------|
| 0x10 | BDM_ENABLE      | Enable BDM via reset sequence (§7.2.1) |
| 0x11 | MEM_READ        | Read target memory                 |
| 0x12 | MEM_WRITE       | Write target memory                |
| 0x13 | REG_READ        | Read A/D register                  |
| 0x14 | REG_WRITE       | Write A/D register                 |
| 0x15 | TARGET_RESET    | Reset target                       |
| 0x16 | TARGET_HALT     | Halt target execution              |
| 0x17 | TARGET_GO       | Resume target execution            |
| 0x18 | STEP            | Single-step target                 |
| 0x19 | BREAKPOINT_SET  | Set hardware breakpoint            |
| 0x1A | BREAKPOINT_CLR  | Clear hardware breakpoint          |
| 0x1B | STATUS          | Query bridge and target status     |
| 0x1C | CONFIG          | Configure bridge parameters        |
| 0x1D | SYSREG_READ     | Read system register (RPC,PCC,SR..) |
| 0x1E | SYSREG_WRITE    | Write system register              |
| 0x1F | MEM_DUMP        | Bulk read memory (auto-increment)  |
| 0x20 | MEM_FILL        | Bulk write memory (auto-increment) |
| 0x21 | CALL            | Call target code at address        |

Command codes start at 0x10 to avoid conflicts with protocol delimiters (STX=0x02, ETX=0x03).

### Response Codes
The response code byte has bit 7 (0x80) set to distinguish responses from commands. The lower 7 bits encode the status:

| Code | Response            | Description                        |
|------|---------------------|------------------------------------|
| 0x00 | RSP_OK              | Command succeeded                  |
| 0x01 | RSP_ERROR           | General error                      |
| 0x02 | RSP_NOT_SUPPORTED   | Unknown command                    |
| 0x03 | RSP_TIMEOUT         | Operation timed out                |
| 0x04 | RSP_TARGET_ERROR    | Target not responding              |

### Checksum
XOR checksum covers all bytes from STX through payload (inclusive), excluding the checksum byte itself and ETX.

## Project Structure

```
.
├── arch/               - Architecture-specific code (stm32f4, avr, target_sim)
├── bridge/             - STM32F411 bridge firmware
│   ├── main.c          - Entry point, command dispatch
│   ├── startup.c       - Reset handler, vector table, SystemInit clock config
│   ├── stm32f411.ld    - Linker script (512K flash, 128K RAM)
│   ├── uart.c/h        - Interrupt-driven UART with ring buffers
│   ├── cli.c/h         - Interactive command-line interface
│   ├── delay.c/h       - Busy-wait microsecond/millisecond delays
│   ├── syscall.c       - Newlib syscall stubs (_write, _read, _sbrk)
│   ├── ringbuf.h       - Lock-free ring buffer
│   └── test/           - Python test suite (pytest, pyserial)
├── bdm_cli.py          - Interactive CLI tool for sending BDM commands
├── common/             - Shared code
├── docs/               - Documentation
├── hal/                - Hardware abstraction layer (legacy)
├── STM32CubeF4/        - CMSIS headers and device support
├── target_sim/         - Software target simulator for testing
└── tests/              - Python test suite (legacy)
```

## Configuration

### Build Configuration (`bridge/config.h`)
- Target MCU: STM32F411CEU6 (Cortex-M4)
- CPU clock: 100 MHz (PLL from HSE 25 MHz or HSI)
- BDM clock frequency: 500 kHz (configurable)
- Serial baud rate: 115200 (configurable)
- Buffer sizes: 256 bytes RX/TX ring buffers

### Pin Mapping (`bridge/config.h`)
| Signal              | Port | Pin | Direction |
|---------------------|------|-----|-----------|
| DSCLK (BKPT)        | A    | 0   | Output    |
| DSI (IFETCH)        | A    | 1   | Output    |
| TARGET_RESET        | A    | 2   | Output    |
| DSO (IPIPE)         | B    | 0   | Input     |
| FREEZE              | B    | 1   | Input     |
| USART1 TX (FTDI)    | B    | 6   | Output    |
| USART1 RX (FTDI)    | B    | 7   | Input     |
| LED                 | C    | 13  | Output    |

## Testing

Run the Python test suite against the flashed bridge (FTDI adapter connected on `/dev/ttyUSB0`):

```sh
make -C bridge test
```

The test suite communicates with the bridge over the FTDI serial connection and validates every response. Without a connected CPU32 target, BDM-dependent commands return `RSP_TARGET_ERROR`, but protocol framing, command parsing, and UART loopback are verified end-to-end.

### Test Cases (22 tests)

| File | Count | Description |
|------|-------|-------------|
| `test_loopback.py` | 14 | Boot message, CLI commands, echo, ping, binary data transfer, buffer boundary tests |
| `test_uart_robustness.py` | 8 | Rapid-fire commands, mixed command sequences, all byte values, ring buffer boundaries |

## Interactive CLI

`bdm_cli.py` provides a readline-based REPL for sending BDM commands to the bridge interactively. It supports tab completion, hex input (`0x` or `$` prefix), and human-readable output.

### Launching

```sh
# Default port (/dev/ttyUSB0) and baud (115200)
python3 bdm_cli.py

# Custom port and baud
python3 bdm_cli.py -p /dev/ttyUSB0 -b 57600

# Via Makefile
make cli-port PORT=/dev/ttyUSB0 BAUD=115200
```

### Command Reference

| Command | Arguments | Description |
|---------|-----------|-------------|
| `enable` | — | Enable BDM via §7.2.1 reset sequence |
| `status` | — | Query bridge and target status |
| `config` | — | Configure bridge parameters |
| `halt` | — | Halt target execution |
| `go` | — | Resume target execution |
| `reset` | — | Reset target |
| `step` | — | Single-step target |
| `mread` | `ADDR [COUNT] [SIZE]` | Read memory (SIZE: 1=byte, 2=word, 4=long) |
| `mwrite` | `ADDR DATA...` | Write memory (comma-separated hex bytes) |
| `mdump` | `ADDR [COUNT] [SIZE]` | Dump memory block with addresses |
| `mfill` | `ADDR COUNT VALUE [SIZE]` | Fill memory block |
| `regread` | `REG` | Read data (0-7) or address (8-15) register |
| `regwrite` | `REG VALUE` | Write data or address register |
| `sysregread` | `SEL` | Read system register (RPC, PCC, SR, etc.) |
| `sysregwrite` | `SEL VALUE` | Write system register |
| `call` | `ADDR` | Call target code at address |
| `bpset` | `ADDR` | Set hardware breakpoint |
| `bpclr` | `ADDR` | Clear hardware breakpoint |
| `raw` | `CMD [DATA...]` | Send raw command byte with optional data |

### Example Session

```
bdm> status
Target in normal mode [OK]

bdm> mread 0x1000 4 4
mread 0x00001000 = 0x00000000 [TARGET_ERROR]

bdm> sysregread 0
RPC = 0x00000000 [TARGET_ERROR]

bdm> quit
Disconnected.
```

### Features

- **Tab completion** — Press Tab to autocomplete command names
- **Hex input** — Accepts `0x1234`, `$1234`, or plain decimal
- **Memory dump formatting** — `mdump` shows address-value pairs
- **Register names** — Registers displayed as `D0`-`D7`, `A0`-`A7`, `RPC`, `PCC`, etc.
- **Raw commands** — `raw` sends arbitrary command bytes for experimentation

## Development Environment

### ARM Toolchain (Bridge Firmware)

- **Compiler**: arm-none-eabi-gcc 15.2.0
- **Components**:
  - arm-none-eabi-gcc 15.2.0
  - arm-none-eabi-gdb
  - arm-none-eabi-ld (GNU Binutils)
- **Build System**: Make
- **Target MCU**: STM32F411CEU6 (Cortex-M4)
- **Install**:
  ```sh
  # Alpine Linux
  apk add gcc-arm-none-eabi newlib-arm-none-eabi g++-arm-none-eabi
  # Debian/Ubuntu
  apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
  ```

### CMSIS Headers

The project uses the official STM32CubeF4 CMSIS headers, located in `STM32CubeF4/`. To fetch them:

```sh
make -C bridge cube
```

Or clone manually:
```sh
git clone --depth 1 https://github.com/STMicroelectronics/STM32CubeF4.git STM32CubeF4
```

## Flashing

### Prerequisites

Install st-flash (from stlink):
```sh
# Alpine Linux
apk add stlink

# Debian/Ubuntu
apt install stlink-tools
```

### Using Makefile

```sh
make -C bridge flash
```

The ST-Link programmer (on-board or external) is used only for flashing. Serial communication with the host uses a separate FTDI adapter connected to USART1 (PB6/PB7). The board resets after flashing.

### Makefile Targets (`bridge/Makefile`)

| Target     | Description                           |
|------------|---------------------------------------|
| all        | Build firmware (hex, elf, size)       |
| flash      | Flash firmware to STM32F411 via ST-Link |
| test       | Run Python test suite                 |
| cli        | Launch interactive BDM CLI            |
| clean      | Remove build artifacts                |

### Build Variants

The Makefile supports multiple build variants for comparing float ABI and stdio configurations:

| Variant       | Float ABI | printf support |
|---------------|-----------|----------------|
| `soft-nofp`   | soft      | none           |
| `soft-fp`     | soft      | float          |
| `hard-nofp`   | hard      | none           |
| `hard-fp`     | hard      | float          |
| `*-stdio`     | as above  | stdio + float  |

Build all variants:
```sh
make -C bridge variants
```

Flash a specific variant:
```sh
make -C bridge flash-hard-fp
```

### Host Toolchain (Target Development)

- m68k-elf-gcc, m68k-elf-gdb (GNU toolchain for Motorola CPU32)
- **OS**: Linux (host side)

## Known Limitations

- BDM clock speed limited by Cortex-M4 instruction timing
- No JTAG support (BDM-only)
- Memory access speed constrained by synchronous BDM protocol
- Breakpoint commands return OK but don't use hardware breakpoint support
- MC68331 target-specific extensions (peripheral debug) not yet implemented
- `bdm_cli.py` is a standalone interactive tool; GDB stub integration is not yet implemented

## References

- Motorola MC68331 Reference Manual
- Motorola CPU32 Background Debug Mode Specification
- GDB Remote Serial Protocol Documentation
- STM32F411 Reference Manual (RM0383)
- STM32F411 Datasheet (DS10693)
