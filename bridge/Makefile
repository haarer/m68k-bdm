PREFIX = /opt/toolchain-arm-none-eabi-current/bin/arm-none-eabi-
CC     = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE   = $(PREFIX)size

COMMON_DIR = ../common

ARCH_FLAGS  = -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
OPT_FLAGS   = -O2 -ffunction-sections -fdata-sections
WARN_FLAGS  = -Wall -Wextra -Werror
CFLAGS      = -std=gnu11 $(ARCH_FLAGS) $(OPT_FLAGS) $(WARN_FLAGS) -ggdb3
LDFLAGS     = -nostartfiles $(ARCH_FLAGS) $(OPT_FLAGS) $(WARN_FLAGS) -ggdb3 \
              -Wl,-gc-sections,-Map,main.map -Wl,--cref -T$(COMMON_DIR)/stm32f411.ld -lc -lgcc

INCLUDES = -I $(COMMON_DIR) \
           -I $(COMMON_DIR)/bdm_engine \
           -I ../STM32CubeF4/Drivers/CMSIS/Include \
           -I ../STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx/Include

.DEFAULT_GOAL := main.hex

COMMON_OBJS = $(COMMON_DIR)/startup.o $(COMMON_DIR)/delay.o $(COMMON_DIR)/uart.o $(COMMON_DIR)/syscall.o \
              $(COMMON_DIR)/bdm_engine/bdm_core.o $(COMMON_DIR)/bdm_engine/bdm_timing.o
BRIDGE_OBJS = main.o cli.o
OBJS = $(COMMON_OBJS) $(BRIDGE_OBJS)

$(OBJS): Makefile $(COMMON_DIR)/stm32f411.ld

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $(INCLUDES) $<

main.elf: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	$(SIZE) $@

main.hex: main.elf
	$(OBJCOPY) -O ihex --set-start 0x08000000 $< $@

flash: main.hex
	st-flash --serial 37FF71064E573436320D1B43 --reset --format ihex write main.hex

# --- Build variants for ABI / float-printf comparison ---
BUILD_DIR = build
OBJ_FILES = $(COMMON_DIR)/startup.o $(COMMON_DIR)/delay.o $(COMMON_DIR)/uart.o $(COMMON_DIR)/syscall.o \
            $(COMMON_DIR)/bdm_engine/bdm_core.o $(COMMON_DIR)/bdm_engine/bdm_timing.o \
            main.o cli.o

ARCH_soft-nofp  := -mthumb -mcpu=cortex-m4 -mfloat-abi=soft
ARCH_soft-fp    := -mthumb -mcpu=cortex-m4 -mfloat-abi=soft
ARCH_hard-nofp  := -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
ARCH_hard-fp    := -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16

DEFS_soft-nofp  :=
DEFS_soft-fp    := -DUSE_FLOAT
DEFS_hard-nofp  :=
DEFS_hard-fp    := -DUSE_FLOAT

EXTRA_soft-nofp :=
EXTRA_soft-fp   := -u _printf_float
EXTRA_hard-nofp :=
EXTRA_hard-fp   := -u _printf_float

# --- Stdio variants (inherit from base, add USE_STDIO) ---
ARCH_soft-nofp-stdio  := $(ARCH_soft-nofp)
ARCH_soft-fp-stdio    := $(ARCH_soft-fp)
ARCH_hard-nofp-stdio  := $(ARCH_hard-nofp)
ARCH_hard-fp-stdio    := $(ARCH_hard-fp)

DEFS_soft-nofp-stdio  := $(DEFS_soft-nofp) -DUSE_STDIO
DEFS_soft-fp-stdio    := $(DEFS_soft-fp) -DUSE_STDIO
DEFS_hard-nofp-stdio  := $(DEFS_hard-nofp) -DUSE_STDIO
DEFS_hard-fp-stdio    := $(DEFS_hard-fp) -DUSE_STDIO

EXTRA_soft-nofp-stdio := $(EXTRA_soft-nofp)
EXTRA_soft-fp-stdio   := $(EXTRA_soft-fp)
EXTRA_hard-nofp-stdio := $(EXTRA_hard-nofp)
EXTRA_hard-fp-stdio   := $(EXTRA_hard-fp)

define variant_rule =
build/$1/$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c | build/$1/$(COMMON_DIR)/
	$$(CC) -c -o $$@ $$(ARCH_$1) $$(OPT_FLAGS) $$(WARN_FLAGS) -ggdb3 $$(DEFS_$1) $$(INCLUDES) $$<

build/$1/$(COMMON_DIR)/bdm_engine/%.o: $(COMMON_DIR)/bdm_engine/%.c | build/$1/$(COMMON_DIR)/bdm_engine/
	$$(CC) -c -o $$@ $$(ARCH_$1) $$(OPT_FLAGS) $$(WARN_FLAGS) -ggdb3 $$(DEFS_$1) $$(INCLUDES) $$<

build/$1/%.o: %.c | build/$1/
	$$(CC) -c -o $$@ $$(ARCH_$1) $$(OPT_FLAGS) $$(WARN_FLAGS) -ggdb3 $$(DEFS_$1) $$(INCLUDES) $$<

build/$1/main.elf: $$(addprefix build/$1/,$$(OBJ_FILES))
	$$(CC) -o $$@ $$^ -nostartfiles $$(ARCH_$1) $$(OPT_FLAGS) $$(WARN_FLAGS) -ggdb3 \
		$$(EXTRA_$1) -Wl,-gc-sections,-Map,build/$1/main.map -Wl,--cref -T$(COMMON_DIR)/stm32f411.ld -lc -lgcc
	$$(SIZE) $$@

build/$1/main.hex: build/$1/main.elf
	$$(OBJCOPY) -O ihex --set-start 0x08000000 $$< $$@

build/$1/:
	mkdir -p $$@

build/$1/$(COMMON_DIR)/:
	mkdir -p $$@

build/$1/$(COMMON_DIR)/bdm_engine/:
	mkdir -p $$@
endef

$(eval $(call variant_rule,soft-nofp))
$(eval $(call variant_rule,soft-fp))
$(eval $(call variant_rule,hard-nofp))
$(eval $(call variant_rule,hard-fp))
$(eval $(call variant_rule,soft-nofp-stdio))
$(eval $(call variant_rule,soft-fp-stdio))
$(eval $(call variant_rule,hard-nofp-stdio))
$(eval $(call variant_rule,hard-fp-stdio))

VARIANTS = soft-nofp soft-fp hard-nofp hard-fp \
           soft-nofp-stdio soft-fp-stdio hard-nofp-stdio hard-fp-stdio

VARIANT_HEXES = $(addprefix $(BUILD_DIR)/,$(addsuffix /main.hex,$(VARIANTS)))

variants: $(VARIANT_HEXES)
	$(SIZE) $(BUILD_DIR)/*/main.elf

variant-sizes: $(VARIANT_HEXES)
	@printf '\n=== Code size by variant ===\n'
	@printf '%-22s %6s %6s %6s %6s\n' Variant text data bss dec
	@for v in $(VARIANTS); do \
	  set -- $$($(SIZE) $(BUILD_DIR)/$$v/main.elf | tail -1); \
	  printf '%-22s %6s %6s %6s %6s\n' $$v $$1 $$2 $$3 $$4; \
	done

soft-nofp: build/soft-nofp/main.hex
soft-fp:   build/soft-fp/main.hex
hard-nofp: build/hard-nofp/main.hex
hard-fp:   build/hard-fp/main.hex
soft-nofp-stdio: build/soft-nofp-stdio/main.hex
soft-fp-stdio:   build/soft-fp-stdio/main.hex
hard-nofp-stdio: build/hard-nofp-stdio/main.hex
hard-fp-stdio:   build/hard-fp-stdio/main.hex

flash-hard-fp: build/hard-fp/main.hex
	st-flash --reset --format ihex write $<

flash-soft-fp: build/soft-fp/main.hex
	st-flash --reset --format ihex write $<

flash-hard-fp-stdio: build/hard-fp-stdio/main.hex
	st-flash --reset --format ihex write $<

flash-soft-fp-stdio: build/soft-fp-stdio/main.hex
	st-flash --reset --format ihex write $<

clean:
	rm -rf *.o *.hex *.elf *.map $(BUILD_DIR)
	rm -f $(COMMON_DIR)/*.o $(COMMON_DIR)/bdm_engine/*.o

test:
	uv sync --directory test
	uv run --directory test python -m pytest -v

.PHONY: clean flash test variants variant-sizes \
        soft-nofp soft-fp hard-nofp hard-fp \
        soft-nofp-stdio soft-fp-stdio hard-nofp-stdio hard-fp-stdio \
        flash-hard-fp flash-soft-fp flash-hard-fp-stdio flash-soft-fp-stdio
