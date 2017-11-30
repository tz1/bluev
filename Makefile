HOSTCC		= gcc -Wall -O6

PRG		= bluev
OBJ		= bluev.o
MCU		= attiny4313
OPTIMIZE	= -Os

DUDE_PRG	= dragon_isp
DUDE_PORT	= usb

DEFS		=
LIBS		=

CC		= avr-gcc

CFLAGS		= --std=c99 -g -Wall $(OPTIMIZE) -mmcu=$(MCU) $(DEFS)
LDFLAGS		= -Wl,-Map,$(PRG).map

OBJCOPY		= avr-objcopy
OBJDUMP		= avr-objdump

all: $(PRG).elf lst text eeprom size v1reShark v1send v1test

v1reShark: v1reShark.c
	$(HOSTCC) v1reShark.c -o $@

v1send: v1send.c
	$(HOSTCC) v1send.c -o $@

v1test: v1test.c
	$(HOSTCC) v1test.c -o $@

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

bluev.o: bluev.c

clean:
	rm -rf *.o
	rm -rf $(PRG).elf $(PRG).bin $(PRG).hex $(PRG).srec
	rm -rf $(PRG)_eeprom.bin $(PRG)_eeprom.hex $(PRG)_eeprom.srec
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)
	rm -rf v1reShark v1send v1test

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

eeprom: ehex ebin esrec

ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $< $@

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $< $@

size: $(PRG).elf
	avr-size  ${PRG}.elf

flash: $(PRG).hex
	avrdude -p t4313 -c $(DUDE_PRG) -P $(DUDE_PORT) -U flash:w:$(PRG).hex

fuse:
#	lowbyte: bits 7	  = CKDIV8	1
#		 bits 6	  = CKOUT	1
#		 bits 5-4 = SUT   0x2 = internal 8 MHz
#				  0x3 = external
#		 bits 3-0 = CKSEL 0x4 = internal 8 MHz
#				  0xe = external > 8 MHz
#	-> 0x64 (default): internal 1 MHz 
#	-> 0xe4 for internal 8 MHz
#	-> 0xfe for external oscillator
	avrdude -p t4313 -c $(DUDE_PRG) -P $(DUDE_PORT) -U lfuse:w:0xfe:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
