# Refactoring Plan: m68k-bdm Project Structure

## Goal

Reorganize the m68k-bdm project so that:

- `bridge/` contains the STM32F411 BDM bridge firmware
- `target_sim/` contains the CPU32 target simulator firmware
- `common/` contains shared STM32F411 platform code and BDM protocol definitions
- `tools/` contains host-side tools (bdm_cli.py, tests)
- No code lives at the m68k-bdm top level (except third-party `STM32CubeF4/` and `docs/`)

Both bridge and target_sim run on **STM32F411 Black Pill** hardware. The bridge and target_sim each use a **separate FTDI USB-to-serial adapter** on USART1 (PB6/PB7) for host communication.

---

## Target Structure

```
m68k-bdm/
├── common/                 # Shared STM32F411 platform + BDM protocol
│   ├── bdm_defs.h          # BDM opcodes, status words, protocol constants
│   ├── startup.c           # Vector table, Reset_Handler, SystemInit (from bridge/)
│   ├── delay.c             # Busy-wait delays (from bridge/)
│   ├── delay.h             # (from bridge/)
│   ├── syscall.c           # Newlib syscall stubs (from bridge/)
│   ├── ringbuf.h           # Inline ring buffer (from bridge/)
│   └── stm32f411.ld        # Linker script (from bridge/)
├── bridge/                 # STM32F411 BDM bridge firmware (working)
│   ├── main.c              # Entry point, BDM command dispatch
│   ├── startup.c           → remove, use common/startup.c
│   ├── delay.c             → remove, use common/delay.c
│   ├── delay.h             → remove, use common/delay.h
│   ├── syscall.c           → remove, use common/syscall.c
│   ├── ringbuf.h           → remove, use common/ringbuf.h
│   ├── stm32f411.ld        → remove, use common/stm32f411.ld
│   ├── uart.c/h            # Keep (USART1, PB6/PB7, FTDI)
│   ├── cli.c/h             # Keep (command-line interface)
│   └── test/               # Keep (Python test suite)
├── target_sim/             # CPU32 target simulator (needs rewrite)
│   ├── main.c              # New entry point (bridge-style)
│   ├── startup.c           → from common/
│   ├── delay.c             → from common/
│   ├── delay.h             → from common/
│   ├── syscall.c           → from common/
│   ├── ringbuf.h           → from common/
│   ├── stm32f411.ld        → from common/
│   ├── uart.c/h            → from common/ (same UART as bridge, PB6/PB7 FTDI)
│   ├── board_config.h      # Keep (BDM pin definitions)
│   ├── sim_bdm.c/h         # Rewrite: replace hal_gpio_* with direct reg access
│   ├── sim_core.c/h        # Keep as-is (pure logic)
│   ├── sim_debug.c/h       # Update: bridge ringbuf + uart instead of hal_serial
│   └── Makefile            # New (same pattern as bridge/Makefile)
├── tools/                  # Host-side tools
│   ├── bdm_cli.py          # Moved from top-level
│   └── tests/              # Moved from top-level
├── STM32CubeF4/            # CMSIS headers (unchanged, third-party)
├── docs/                   # Documentation
│   └── REFACTOR_PLAN.md    # This file
└── README.md
```

---

## Execution Order

### Phase 1 — Create `common/`

1. Copy from `bridge/`:
   - `startup.c` → `common/startup.c`
   - `delay.c` → `common/delay.c`
   - `delay.h` → `common/delay.h`
   - `syscall.c` → `common/syscall.c`
   - `ringbuf.h` → `common/ringbuf.h`
   - `stm32f411.ld` → `common/stm32f411.ld`

2. Create `common/bdm_defs.h` with:
   - BDM opcodes (NOP, RST, GO, CALL, RAREG, WAREG, RSREG, WSREG, READ, WRITE, DUMP, FILL)
   - BDM operand size encodings (byte, word, long)
   - BDM status words (OK=0xFFFF, BERR=0x8001, ILLEGAL=0x0001, NOT_READY)
   - System register select codes (RPC, PCC, SR, USP, SSP, SFC, DFC, ATEMP, FAR, VBR)
   - Host protocol STX/ETX delimiters
   - Command codes (CMD_BDM_ENABLE .. CMD_CALL)
   - Response codes (RSP_OK .. RSP_TARGET_ERROR)

### Phase 2 — Create `tools/`

1. Move `bdm_cli.py` → `tools/bdm_cli.py`
2. Move `tests/` → `tools/tests/`

### Phase 3 — Clean up old infrastructure

Delete:
- `hal/` — old AVR-era HAL abstraction
- `arch/` — old architecture implementations (AVR, STM32F1, STM32F4, target_sim)
- `common/` (old) — old AVR-era shared code
- `bdm_cli.py` (top-level, already moved)
- `tests/` (top-level, already moved)

### Phase 4 — Update bridge to use `common/`

1. Remove from `bridge/`:
   - `startup.c`
   - `delay.c`, `delay.h`
   - `syscall.c`
   - `ringbuf.h`
   - `stm32f411.ld`

2. Update `bridge/Makefile`:
   - Add `-I ../common` to INCLUDES
   - Change object paths: `../common/startup.o`, `../common/delay.o`, `../common/syscall.o`
   - Change linker script path: `-T../common/stm32f411.ld`
   - Remove `ringbuf.h` from OBJS (it's a header-only inline)

3. Build bridge and verify all tests pass.

### Phase 5 — Port target_sim to bridge-style

1. Create `target_sim/startup.c` → symlink or reference `common/startup.c`
2. Create `target_sim/delay.c/h` → reference `common/delay.c/h`
3. Create `target_sim/syscall.c` → reference `common/syscall.c`
4. Create `target_sim/ringbuf.h` → reference `common/ringbuf.h`
5. Create `target_sim/stm32f411.ld` → reference `common/stm32f411.ld`
6. Create `target_sim/Makefile` — same pattern as bridge/Makefile but with `target_sim` objects

7. **Rewrite `sim_bdm.c`**: Replace `hal_gpio_*()` calls with direct STM32F411 register access:
   - `hal_gpio_set_output(port, pin)` → GPIO port MODER, OTYPER, OSPEEDR, PUPDR manipulation
   - `hal_gpio_set_input_pullup(port, pin)` → MODER + PUPDR
   - `hal_gpio_set_high(port, pin)` → BSRR
   - `hal_gpio_set_low(port, pin)` → BSRR (bit 16)
   - `hal_gpio_read(port, pin)` → IDR
   - `hal_delay_us(us)` → common/delay.h `delay_us()`
   - `sim_bdm_init()` → use `RCC->AHB1ENR` to enable GPIO clocks, then configure pins
   - Use `board_config.h` constants for port/pin numbers (PORT_A=0, PORT_B=1, etc.)

8. **Update `sim_debug.c`**:
   - Replace `ringbuf_push`/`ringbuf_pop`/`ringbuf_init`/etc with bridge's inline ring buffer API (`ringbuffer_put_head`, `ringbuffer_get_tail`, etc.)
   - Replace `hal_serial_try_putc(char)` with `uart_putc(char)` from common UART
   - Replace `hal_serial_init(0)` with `uart_init()` from common

9. **Create `target_sim/main.c`** — bridge-style entry point:
   - `main()`: init UART, init debug logger, init sim_core, loop: `sim_core_run()` + drain debug buffer
   - Add LED blink pattern for boot indication (like bridge does)

10. **Create `target_sim/uart.c/h`** — copy from `bridge/uart.c/h` (same USART1 on PB6/PB7)

### Phase 6 — Update README

Update `README.md` to reflect the new structure.

---

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Ring buffer API | Bridge's inline header (`ringbuffer_put_head`, `ringbuffer_get_tail`) | Working and proven |
| Startup code | Bridge's `startup.c` (C-based, 100 MHz PLL) | Only valid implementation |
| UART | USART1 on PB6/PB7, FTDI adapter | Same for both bridge and target_sim |
| Hardware abstraction | Direct register access (no HAL) | Bridge pattern, simpler and more transparent |
| USB CDC | Not used | Broken; both projects use FTDI UART |
| Build system | Make (same pattern as bridge) | Consistent across both projects |

## BDM Pin Mapping (both bridge and target_sim)

| Signal | Port | Pin | Direction (bridge) | Direction (target_sim) |
|--------|------|-----|--------------------|----------------------|
| DSCLK (BKPT) | A | 0 | Output | Input (with pull-up) |
| DSI (IFETCH) | A | 1 | Output | Input (with pull-up) |
| DSO (IPIPE) | B | 0 | Input | Output |
| FREEZE | B | 1 | Input | Output |
| TARGET_RESET | A | 2 | Output | Input (with pull-up) |
| USART1 TX | B | 6 | Output | Output |
| USART1 RX | B | 7 | Input | Input |
| LED | C | 13 | Output | Output |
