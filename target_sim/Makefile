PREFIX = /usr/bin/arm-none-eabi-
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

INCLUDES = -I . -I $(COMMON_DIR) \
           -I ../STM32CubeF4/Drivers/CMSIS/Include \
           -I ../STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx/Include

.DEFAULT_GOAL := main.hex

COMMON_OBJS = $(COMMON_DIR)/startup.o $(COMMON_DIR)/delay.o $(COMMON_DIR)/uart.o $(COMMON_DIR)/syscall.o
TARGET_OBJS = main.o sim_bdm.o sim_core.o sim_debug.o
OBJS = $(COMMON_OBJS) $(TARGET_OBJS)

$(OBJS): Makefile $(COMMON_DIR)/stm32f411.ld

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $(INCLUDES) $<

main.elf: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	$(SIZE) $@

main.hex: main.elf
	$(OBJCOPY) -O ihex --set-start 0x08000000 $< $@

flash: main.hex
	st-flash --reset --format ihex write main.hex

clean:
	rm -rf *.o *.hex *.elf *.map

.PHONY: clean flash
