# ------------------------------------------------------------------ #
#  m68k-bdm — Multi-architecture BDM bridge                          #
#  Usage: make VARIANT=avr|stm32f1|target_sim [all|clean|flash|size] #
#  Build outputs go to build/$(VARIANT)/                              #
# ------------------------------------------------------------------ #

VARIANT ?= avr

# ------------------------------------------------------------------ #
#  Directory layout                                                   #
# ------------------------------------------------------------------ #

COMMON   := common
HAL      := hal
ARCH     := arch/$(VARIANT)
TESTS    := tests
BUILDDIR := build/$(VARIANT)
TARGET_SIM := target_sim

# Target name depends on variant
ifeq ($(VARIANT),target_sim)
    TARGET := $(BUILDDIR)/bdm_target_sim
else
    TARGET := $(BUILDDIR)/bdm_bridge
endif

# ------------------------------------------------------------------ #
#  Variant-specific configuration                                     #
# ------------------------------------------------------------------ #

ifeq ($(VARIANT),avr)
    TOOLCHAIN  := /opt/toolchain-avr-current/bin
    CC         := $(TOOLCHAIN)/avr-gcc
    OBJCOPY    := $(TOOLCHAIN)/avr-objcopy
    SIZE       := $(TOOLCHAIN)/avr-size
    OD         := $(TOOLCHAIN)/avr-objdump

    DEVICE     := atmega2560
    F_CPU      := 16000000UL

    CFLAGS_ARCH := -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)
    LDFLAGS    := -mmcu=$(DEVICE)

    ARCH_SRCS  := $(ARCH)/hal_uart.c

    FLASH_TOOL := avrdude
    FLASH_PORT ?= /dev/ttyACM0
    FLASH_BAUD ?= 115200
    FLASH_PROG ?= wiring
    FLASH_ARGS := -c $(FLASH_PROG) -p $(DEVICE) -P $(FLASH_PORT) -b $(FLASH_BAUD) -D

    SIZE_FMT   := --format=avr

else ifeq ($(VARIANT),stm32f1)
    TOOLCHAIN  := /opt/toolchain-arm-none-eabi-current/bin
    CC         := $(TOOLCHAIN)/arm-none-eabi-gcc
    OBJCOPY    := $(TOOLCHAIN)/arm-none-eabi-objcopy
    SIZE       := $(TOOLCHAIN)/arm-none-eabi-size
    OD         := $(TOOLCHAIN)/arm-none-eabi-objdump

    CFLAGS_ARCH := -mcpu=cortex-m3 -mthumb -DF_CPU=72000000UL
    LDFLAGS    := -mcpu=cortex-m3 -mthumb -T $(ARCH)/stm32f103c8.ld -nostartfiles

    ARCH_SRCS  := $(ARCH)/hal_usb_cdc.c $(ARCH)/startup_stm32f103xb.S

    FLASH_TOOL := st-flash
    FLASH_ARGS := write

    SIZE_FMT   :=

else ifeq ($(VARIANT),target_sim)
    TOOLCHAIN  := /opt/toolchain-arm-none-eabi-current/bin
    CC         := $(TOOLCHAIN)/arm-none-eabi-gcc
    OBJCOPY    := $(TOOLCHAIN)/arm-none-eabi-objcopy
    SIZE       := $(TOOLCHAIN)/arm-none-eabi-size
    OD         := $(TOOLCHAIN)/arm-none-eabi-objdump

    CFLAGS_ARCH := -mcpu=cortex-m3 -mthumb -DF_CPU=72000000UL
    LDFLAGS    := -mcpu=cortex-m3 -mthumb -T arch/stm32f1/stm32f103c8.ld -nostartfiles

    # Target simulator uses its own sources + shared STM32 HAL + startup
    ARCH_SRCS  := arch/stm32f1/hal_usb_cdc.c arch/stm32f1/startup_stm32f103xb.S
    SIM_SRCS   := $(TARGET_SIM)/main.c $(TARGET_SIM)/sim_core.c $(TARGET_SIM)/sim_bdm.c

    FLASH_TOOL := st-flash
    FLASH_ARGS := write

    SIZE_FMT   :=

else
    $(error Unknown VARIANT=$(VARIANT). Use avr, stm32f1, or target_sim)
endif

# ------------------------------------------------------------------ #
#  Sources, objects, and flags                                        #
# ------------------------------------------------------------------ #

ifeq ($(VARIANT),target_sim)
    SRCS := $(SIM_SRCS) $(ARCH_SRCS) $(COMMON)/utils/ringbuf.c
else
    COMMON_SRCS := $(COMMON)/main.c \
                   $(COMMON)/serial/protocol.c \
                   $(COMMON)/bdm_engine/bdm_core.c \
                   $(COMMON)/bdm_engine/bdm_timing.c \
                   $(COMMON)/utils/ringbuf.c \
                   $(COMMON)/utils/checksum.c

    SRCS := $(COMMON_SRCS) $(ARCH_SRCS)
endif

SRCS_C := $(filter %.c,$(SRCS))
SRCS_S := $(filter %.S,$(SRCS))

OBJS := $(patsubst %.c,$(BUILDDIR)/%.o,$(SRCS_C)) \
        $(patsubst %.S,$(BUILDDIR)/%.o,$(SRCS_S))

CFLAGS := $(CFLAGS_ARCH)
CFLAGS += -Os -g2 -std=c11
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -I$(HAL) -I$(ARCH) -I$(COMMON) -I$(COMMON)/utils
ifeq ($(VARIANT),target_sim)
    CFLAGS += -I$(TARGET_SIM)
else
    CFLAGS += -I$(COMMON)/serial -I$(COMMON)/bdm_engine
endif

# ------------------------------------------------------------------ #
#  Build targets                                                      #
# ------------------------------------------------------------------ #

all: $(TARGET).hex $(TARGET).bin size

$(BUILDDIR):
ifeq ($(VARIANT),target_sim)
	mkdir -p $(BUILDDIR)/$(TARGET_SIM) $(BUILDDIR)/arch/stm32f1 $(BUILDDIR)/common/utils
else
	mkdir -p $(BUILDDIR)/$(COMMON)/serial $(BUILDDIR)/$(COMMON)/bdm_engine \
	         $(BUILDDIR)/$(COMMON)/utils $(BUILDDIR)/$(ARCH)
endif

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	@echo "---> compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: %.S | $(BUILDDIR)
	@echo "---> assembling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	@echo "---> linking $(VARIANT)..."
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

size: $(TARGET).elf
	$(SIZE) $(SIZE_FMT) $<

flash: $(TARGET).hex $(TARGET).bin
ifeq ($(VARIANT),avr)
	$(FLASH_TOOL) $(FLASH_ARGS) -U flash:w:$(TARGET).hex:i
else
	$(FLASH_TOOL) $(FLASH_ARGS) $(TARGET).bin 0x08000000
endif

clean:
	rm -rf build

disasm: $(TARGET).elf
	$(OD) -d -S $< > $(TARGET).lst

# ------------------------------------------------------------------ #
#  Test (AVR only for now)                                            #
# ------------------------------------------------------------------ #

VENV     := venv
PYTHON   := $(VENV)/bin/python3

$(VENV)/bin/python3:
	python3 -m venv $(VENV)

test-deps: $(VENV)/bin/python3
	@uv pip install --python $(PYTHON) --quiet pyserial

test: test-deps
ifeq ($(VARIANT),avr)
	$(PYTHON) $(TESTS)/test_bdm_bridge.py
else ifeq ($(VARIANT),stm32f1)
	$(PYTHON) $(TESTS)/test_hil.py
else
	@echo "Tests only supported for avr and stm32f1 variants"
endif

test-hil: test-deps
	$(PYTHON) $(TESTS)/test_hil.py

help:
	@echo "m68k-bdm — Multi-architecture BDM bridge"
	@echo ""
	@echo "Usage: make VARIANT=<target> [target]"
	@echo ""
	@echo "Variants:"
	@echo "  avr         Arduino Mega 2560 (ATmega2560, UART host comms)"
	@echo "  stm32f1     Blackpill STM32F103C8T6 (USB CDC host comms)"
	@echo "  target_sim  Blackpill CPU32 BDM target simulator"
	@echo ""
	@echo "Targets:"
	@echo "  all         Build firmware (elf, hex, bin) + size report"
	@echo "  clean       Remove build/ directory"
	@echo "  flash       Flash to target (avrdude / st-flash)"
	@echo "  size        Print binary size"
	@echo "  disasm      Generate disassembly listing"
	@echo "  test        Run test suite (AVR protocol / STM32 HIL)"
	@echo "  test-hil    Run hardware-in-the-loop tests"
	@echo ""
	@echo "Examples:"
	@echo "  make VARIANT=avr all"
	@echo "  make VARIANT=stm32f1 flash"
	@echo "  make VARIANT=target_sim all"
	@echo "  make VARIANT=stm32f1 test-hil"

.PHONY: all clean flash size disasm test test-deps test-hil help
