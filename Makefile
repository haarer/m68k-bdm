TOOLCHAIN  := /opt/toolchain-avr-current/bin
CC         := $(TOOLCHAIN)/avr-gcc
OBJCOPY    := $(TOOLCHAIN)/avr-objcopy
SIZE       := $(TOOLCHAIN)/avr-size
OD         := $(TOOLCHAIN)/avr-objdump

DEVICE     := atmega2560
F_CPU      := 16000000UL

CFLAGS     := -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)
CFLAGS     += -Os -g2 -std=c11
CFLAGS     += -Wall -Wextra -Wno-unused-parameter
CFLAGS     += -I. -Ibdm_engine -Iserial -Ihal -Iutils

LDFLAGS    := -mmcu=$(DEVICE)

SRCDIR     := .
BDMDIR     := bdm_engine
SERDIR     := serial
HALDIR     := hal
UTILDIR    := utils

SRCS       := main.c
SRCS       += $(BDMDIR)/bdm_core.c $(BDMDIR)/bdm_timing.c
SRCS       += $(SERDIR)/uart.c $(SERDIR)/protocol.c
SRCS       += $(HALDIR)/pins.c
SRCS       += $(UTILDIR)/ringbuf.c $(UTILDIR)/checksum.c

OBJS       := $(SRCS:.c=.o)

TARGET     := bdm_bridge

AVRDUDE    := avrdude
PORT       ?= /dev/ttyACM0
BAUD       ?= 115200
PROG       ?= wiring

all: $(TARGET).hex $(TARGET).eep size

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

%.eep: $(TARGET).elf
	$(OBJCOPY) -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load \
		--no-change-warnings --gap-fill=0xFF $< $@

size: $(TARGET).elf
	$(SIZE) --format=avr $<

flash: $(TARGET).hex
	$(AVRDUDE) -c $(PROG) -p $(DEVICE) -P $(PORT) -b $(BAUD) -D -U flash:w:$<:i

eeprom: $(TARGET).eep
	$(AVRDUDE) -c $(PROG) -p $(DEVICE) -P $(PORT) -b $(BAUD) -D -U eeprom:w:$<:i

fuse:
	$(AVRDUDE) -c $(PROG) -p $(DEVICE) -P $(PORT) -b $(BAUD) \
		-U hfuse:w:0xD2:m -U lfuse:w:0xFF:m

clean:
	rm -f $(TARGET).elf $(TARGET).hex $(TARGET).eep
	rm -f $(SRCDIR)/*.o $(BDMDIR)/*.o $(SERDIR)/*.o $(HALDIR)/*.o $(UTILDIR)/*.o

disasm: $(TARGET).elf
	$(OD) -d -S $< > $(TARGET).lst

upload: flash

.PHONY: all clean flash eeprom fuse size disasm upload
