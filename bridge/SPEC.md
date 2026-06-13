# Specification — STM32F411 Blackpill Bare Metal Firmware

## Goal

A minimal bare-metal firmware image that configures the necessary hardware
and prints "hello world" on the serial console. Then it provides a simple command line allowing to turn the led on and off.

## Hardware

- **Board:** Blackpill V3.1 (WeAct Studio)
  - [WeActTC/MiniF4-STM32F4x1](https://github.com/WeActTC/MiniF4-STM32F4x1)
  - Pinout is identical to V2.0
  - PCB: 20.78 × 52.81 mm, black, breadboard-friendly
- **MCU:** STM32F411CEU6 (Cortex-M4F, 512K flash, 128K SRAM, UFQFPN48)
  - Reference manual: RM0383 (see `docs/`)
- **Oscillators:** HSE 25 MHz, HSI 16 MHz, LSE 32.768 kHz, LSI 32 kHz
- **Regulator:** AP7343 (+3.3V @ 300 mA)
- **LEDs:** PC13 (user, active-low sink), power LED
- **Button:** PA0 (KEY, active-low)
- **Programmer:** ST-Link V2 via SWD header (bottom edge)
- **Serial:** FTDI adapter connected to PB6/PB7 (USART1)
- **USB:** USB-C connector (PA11 DM, PA12 DP)

### Pinout

Orientation: USB-C at top, SWD at bottom, components facing up.

#### Right header (Header 1)

| # | Label | MCU pin | Notes |
|---|-------|---------|-------|
| 1 | 5V    | —       | +5V rail (USB, no protection) |
| 2 | G     | —       | Ground |
| 3 | 3.3   | —       | +3.3V rail (regulated) |
| 4 | B10   | PB10    | |
| 5 | B2    | PB2     | |
| 6 | B1    | PB1     | |
| 7 | B0    | PB0     | |
| 8 | A7    | PA7     | |
| 9 | A6    | PA6     | |
| 10 | A5   | PA5     | |
| 11 | A4   | PA4     | EEPROM /CS |
| 12 | A3   | PA3     | |
| 13 | A2   | PA2     | |
| 14 | A1   | PA1     | |
| 15 | A0   | PA0     | KEY button (active low) |
| 16 | R    | NRST    | Reset (active low) |
| 17 | C15  | PC15    | |
| 18 | C14  | PC14    | |
| 19 | C13  | PC13    | User LED (sink, active low) |
| 20 | VB   | VBAT    | Backup battery |

#### Left header (Header 2)

| # | Label | MCU pin | Notes |
|---|-------|---------|-------|
| 1 | B12   | PB12    | |
| 2 | B13   | PB13    | |
| 3 | B14   | PB14    | |
| 4 | B15   | PB15    | |
| 5 | A8    | PA8     | |
| 6 | A9    | PA9     | USART1_TX (AF7) |
| 7 | A10   | PA10    | USART1_RX (AF7) |
| 8 | A11   | PA11    | USB_DM |
| 9 | A12   | PA12    | USB_DP |
| 10 | A15  | PA15    | |
| 11 | B3   | PB3     | |
| 12 | B4   | PB4     | EEPROM DO |
| 13 | B5   | PB5     | |
| 14 | B6   | PB6     | **USART1_TX** (AF7) — connected to FTDI RX |
| 15 | B7   | PB7     | **USART1_RX** (AF7) — connected to FTDI TX |
| 16 | B8   | PB8     | |
| 17 | B9   | PB9     | |
| 18 | 5V   | —       | +5V rail |
| 19 | G    | —       | Ground |
| 20 | 3.3  | —       | +3.3V rail |

#### SWD header (bottom edge)

| # | Label | MCU pin | Notes |
|---|-------|---------|-------|
| 1 | 3.3V  | —       | +3.3V |
| 2 | SWDIO | PA13    | |
| 3 | SWCLK | PA14    | |
| 4 | GND   | —       | Ground |

## Boot Sequence & Main Loop

1. **Clock init** — HSE→PLL→100 MHz with flash wait states and Scale 1 voltage
2. **5 fast LED blinks** — PC13 toggles 5 times at 80 ms on/off (visual alive check)
3. **UART init** — USART1 on PB6/PB7 at 115200 8N1
4. **Print** `hello world\n`
5. **Print** `stdio connected via interrupt-based UART with ring buffers\n`
6. **CLI init** — line buffer cleared, prompt `> ` printed
7. **Main loop** — polls UART every iteration for commands. No LED blink during CLI operation.

## Software Constraints

- **Bare metal** — no STM HAL libraries
- **CMSIS only** — clone from https://github.com/STMicroelectronics/STM32CubeF4
- **UART1** on PB6 (TX) / PB7 (RX), 115200 8N1, via AF7
- **Toolchain:** `arm-none-eabi` via `tcman` (see toolchain skill)
- **Makefile** — maintain a `Makefile` for building and flashing

## Development Workflow

- The workspace (`/workspace`) is shared with the host
- The user validates and commits; I do not commit
- Each development iteration is recorded in `progress.md`
  - Includes goal, what was done, files created/modified, and key decisions

## Environment

- Running inside a Podman container
- Install any needed tools via `apk` (Alpine Linux)
- Connected hardware: ST-Link V2 programmer, FTDI adapter on PB6/PB7 (USART1)

## Hardware Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                      Host Computer                           │
│  ┌───────────────────────────────────────────────────────┐   │
│  │                 Podman Container                      │   │
│  │                    opencode                           │   │
│  └────────────────────────┬──────────────────────────────┘   │
│                           │                                  │
│                           ▼                                  │
│                    ┌──────────────┐                          │
│                    │     USB      │                          │
│                    └──────┬───────┘                          │
└───────────────────────────┼──────────────────────────────────┘
                            │
               ┌────────────┼────────────┐
               │                         │
               ▼                         ▼
       ┌───────────────┐        ┌──────────────┐
       │ ST-Link V2    │        │ FTDI FT232R  │
       │ Programmer    │        │              │
       └───────┬───────┘        └──────┬───────┘
               │ SWD                   │ UART
               ▼                       ▼
       ┌──────────────────────────────────────┐
       │    Blackpill V3.1 (STM32F411CEU6)    │
       │                                      │
       └──────────────────────────────────────┘
```

### Interconnections

| Interface | Host side | Adapter | Blackpill pin | Notes |
|-----------|-----------|---------|---------------|-------|
| USB       | `/dev/bus/usb` | ST-Link V2 | — | libusb enumeration for st-flash |
| USB       | `/dev/ttyUSB0` | FTDI FT232R | — | serial over USB CDC |
| SWD       | — | ST-Link SWD header pin 2 | PA13 (SWDIO) | programming/debug data |
| SWD       | — | ST-Link SWD header pin 3 | PA14 (SWCLK) | programming/debug clock |
| SWD       | — | ST-Link SWD header pin 4 | GND | common ground |
| UART      | — | FTDI TX | PB7 (USART1_RX) | MCU receive |
| UART      | — | FTDI RX | PB6 (USART1_TX) | MCU transmit |
| UART      | — | FTDI GND | GND | common ground |

## Testing

### Loopback test

Verify the firmware outputs the expected string on the serial console:

1. Open the FTDI serial port **before** resetting the MCU (the firmware sends the boot banner ~800 ms after boot (5 blinks × 160 ms + UART init), so opening after reset will miss it)
2. Reset the MCU via `st-flash reset`
3. Read from the serial port and assert the output matches `hello world\n\rstdio connected via interrupt-based UART with ring buffers\n\r> `

The test suite lives in `test/` and is run via `make test`:

```bash
make test
```

### CLI commands

After boot the firmware accepts these commands via UART:

| Command | Response | Description |
|---------|----------|-------------|
| `help` | list of commands | Show available commands |
| `hello` | `hello world` | Print greeting |
| `ping` | `pong` | Liveness check |
| `led on` | `ok` | Turn PC13 LED on |
| `led off` | `ok` | Turn PC13 LED off |
| `echo <text>` | `<text>` | Echo back the argument |
| `echobin <n>` | `ok` + `n` raw bytes | Echo back `n` binary bytes from host |
| `pktsend <n>` | `ok` + `n` byte pattern | Send `n` bytes of counting pattern `(i & 0xFF)` |
| *(empty line)* | — | ignored, prompt re-printed |

### Test suite

Located in `test/` and managed with `uv`:

```
test/
├── conftest.py       # session-scoped flash via pytest_sessionstart hook
├── pyproject.toml    # uv project config
├── uv.lock           # pinned dependencies
├── main.py           # uv init scaffold (unused)
└── test_loopback.py  # 14 loopback test cases
```

The session hook in `conftest.py` flashes the firmware once via `st-flash --reset` before any tests run. Each test then calls `st-flash reset` independently as a precondition, opening the serial port before resetting to capture the boot output.

Tests are run via `make test` and cover the following:

| Test | What it verifies |
|------|-----------------|
| `test_boot_message` | Full boot output matches expected string |
| `test_hello_command` | `hello` → `hello world` |
| `test_echo_command` | `echo <text>` → `<text>` |
| `test_led_on_off` | `led on` / `led off` → `ok` |
| `test_help_command` | lists all commands |
| `test_unknown_command` | garbage input → `error: unknown command` |
| `test_ping` | `ping` → `pong` |
| `test_empty_line` | bare `\n` → echo + prompt, no error |
| `test_echobin_small` | `echobin 100` → 100 raw bytes echoed back |
| `test_pktsend_small` | `pktsend 10` — small payload |
| `test_pktsend_buffer_fill` | `pktsend 255` — exactly fills ring buffer |
| `test_pktsend_exceed_buffer` | `pktsend 256` — one past buffer capacity |
| `test_pktsend_large` | `pktsend 512` — wraps pattern twice, stress test |
| `test_long_line_truncation` | 66-char line → truncated to 63 chars |

Python dependency management uses `uv`. Add new packages with:

```bash
uv add --directory test <package>
```

Add dev-only packages (e.g. test runners) with:

```bash
uv add --dev --directory test <package>
```

The `pyproject.toml` specifies the project metadata and dependencies. Lockfiles ensure reproducible test environments.

### Host configuration (udev)

The container uses a user namespace (root in container maps to a non-root UID on the host). USB device nodes created by udev with restrictive permissions (e.g. `nobody:nobody`, mode `660`) are inaccessible to the container root because the owning UID/GID is unmapped.

**Fix:** Create udev rules on the host that make the devices world-accessible or owned by the correct host UID:

```bash
# ST-Link V2 — libusb access via /dev/bus/usb
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-stlink.rules

# FTDI FT232R — serial port access
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-ftdi.rules

sudo udevadm control --reload-rules
# Unplug and re-plug both devices, or trigger:
sudo udevadm trigger
```

### Podman container configuration

Pass both USB devices into the container. Prefer `--device` for the tty device (creates the node with correct perms inside the container) and a bind mount for the USB bus (libusb enumeration):

```bash
podman run -it \
    -v "$(pwd)/workspace:/workspace" \
    --device /dev/ttyUSB0:rw \
    --device /dev/bus/usb:/dev/bus/usb \
    --privileged \
    --group-add keep-groups \
    --security-opt label=disable \
    --name stm32-dev \
    ghcr.io/anomalyco/opencode:latest
```

| Flag | Purpose |
|------|---------|
| `--device /dev/ttyUSB0:rw` | Passes the FTDI serial device with correct cgroup and permission handling |
| `--device /dev/bus/usb:/dev/bus/usb` | Mounts the USB filesystem so libusb can enumerate devices (needed by st-flash) |
| `--privileged` | Grants full capabilities and device access (simpler than enumerating each device cgroup) |
| `--group-add keep-groups` | Preserves supplementary groups from the host, useful when udev assigns device nodes to specific groups |
| `--security-opt label=disable` | Disables SELinux/AppArmor label confinement inside the container |

Without `--device /dev/ttyUSB0:rw`, the tty device node is inherited from the host bind-mount (`-v /dev:/dev`) with the host's original permissions and ownership, which may be inaccessible due to UID/GID unmapping in the user namespace.
