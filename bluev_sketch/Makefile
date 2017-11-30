HOSTCC		= gcc -Wall -O6

PRG		= bluevmega
OBJ		= bluevmega.o bluevirq.o
MCU		= atmega1280
OPTIMIZE	= -Os

DUDE_PRG	= stk500v1
DUDE_PORT	= /dev/ttyUSB0

DEFS		= -DSTANDALONE
LIBS		=

CC		= avr-gcc

CFLAGS		= --std=c99 -g -Wall $(OPTIMIZE) -mmcu=$(MCU) $(DEFS)
LDFLAGS		= -Wl,-Map,$(PRG).map

OBJCOPY		= avr-objcopy
OBJDUMP		= avr-objdump

all: $(PRG).elf lst text eeprom size

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

bluevmega.o: bluevmega.c

bluevirq.o: bluevirq.c

clean:
	rm -rf *.o
	rm -rf $(PRG).elf $(PRG).bin $(PRG).hex $(PRG).srec
	rm -rf $(PRG)_eeprom.bin $(PRG)_eeprom.hex $(PRG)_eeprom.srec
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)

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
	avrdude -F -p m1280 -c $(DUDE_PRG) -P $(DUDE_PORT) -b 57600 -U flash:w:$(PRG).hex
