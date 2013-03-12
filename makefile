CC=avr-gcc
CFLAGS=-Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU)
MCU=attiny84
F_CPU=8000000UL
PROJECT=spi

AVRSIZE=avr-size
OBJDUMP=avr-objdump

OBJCOPY=avr-objcopy
BIN_FORMAT=ihex

PORT=usb
//PROTOCOL=avrispmkII
PROTOCOL=usbasp
PART=$(MCU)
AVRDUDE=avrdude -F -V
AVRSIZE=avr-size


RM=rm -f

.PHONY: all
all: $(PROJECT).hex

$(PROJECT).hex: $(PROJECT).elf

$(PROJECT).elf: $(PROJECT).s

$(PROJECT).s: $(PROJECT).c

.PHONY: clean
clean:
	$(RM) $(PROJECT).elf $(PROJECT).hex $(PROJECT).s $(PROJECT).disasm

.PHONY: upload
upload: $(PROJECT).hex
	$(AVRDUDE) -c $(PROTOCOL) -p $(PART) -P $(PORT) -U flash:w:$<

size:   $(PROJECT).elf
	$(AVRSIZE) $<

disasm:	$(PROJECT).elf
	$(OBJDUMP) -S $< > $(PROJECT).disasm

rfuses: 
	$(AVRDUDE) -c $(PROTOCOL) -P $(PORT) -p $(PART) -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

wfuses:
	$(AVRDUDE) -c $(PROTOCOL) -P $(PORT) -p $(PART) -U lfuse:w:0xe2:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m

%.elf: %.s ; $(CC) $(CFLAGS) -s -o $@ $<

%.s: %.c ; $(CC) $(CFLAGS) -S -o $@ $<

%.hex: %.elf ; $(OBJCOPY) -O $(BIN_FORMAT) -R .eeprom $< $@
