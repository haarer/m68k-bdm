# STM32F411 Blackpill Bare Metal Firmware

A minimal bare-metal firmware for the WeAct Studio Blackpill V3.1
(STM32F411CEU6). Prints `hello world` on USART1 and blinks the user LED.

## Project

| Artifact | Description |
|----------|-------------|
| [SPEC.md](SPEC.md) | Full specification, pinout, architecture, testing, and environment setup |
| [progress.md](progress.md) | Development log by iteration |

## Development Ecosystem

This project is developed inside an [opencode](https://opencode.ai) container
running on the host via Podman. The container launch script is at
[github.com/haarer/opencode](https://github.com/haarer/opencode), using the
image `ghcr.io/anomalyco/opencode:latest`.

```
Host ── Podman ── opencode ── USB ── ST-Link V2 ── SWD ── Blackpill
                                    └── FTDI FT232R ── UART ──┘
```

### Toolchain

The `arm-none-eabi` cross-compiler is installed via
[tcman](https://github.com/haarer/tcman), which downloads prebuilt GCC/Binutils/GDB
releases from [toolchain68k](https://github.com/haarer/toolchain68k).

### CMSIS

CMSIS device headers are pulled from
[STM32CubeF4](https://github.com/STMicroelectronics/STM32CubeF4)
via `make cube` (git clone + submodule).

### Stlink

Flashing and debug access use [stlink](https://github.com/stlink-org/stlink)
(`st-flash`, `st-info`).

### Test Suite

Python-based tests managed with [uv](https://docs.astral.sh/uv/).
Run with `make test`.

## Quick Start

```bash
# Install toolchain
sudo tcman install arm-none-eabi

# Fetch CMSIS headers
make cube

# Build
make

# Flash
make flash

# Test
make test
```

See [SPEC.md](SPEC.md) for host udev rules and Podman configuration.
