PREFIX = /opt/toolchain-arm-none-eabi-current/bin/arm-none-eabi-
CC     = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE   = $(PREFIX)size

.DEFAULT_GOAL := all

all: bridge target_sim

bridge:
	$(MAKE) -C bridge

target_sim:
	$(MAKE) -C target_sim

flash-bridge:
	$(MAKE) -C bridge flash

flash-target_sim:
	$(MAKE) -C target_sim flash

test-bridge:
	$(MAKE) -C bridge test

clean:
	$(MAKE) -C bridge clean
	$(MAKE) -C target_sim clean
	rm -f ../m68k-bdm/target_sim.bin 2>/dev/null; true

.PHONY: all bridge target_sim flash-bridge flash-target_sim test-bridge clean
