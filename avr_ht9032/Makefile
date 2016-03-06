#DEVICE	    	=atmega328
#DEVICE	    	=attiny85
DEVICE	    	=attiny84
PROGRAMMER  	=usbasp
AVRDUDE_OPTS	=-B3
AVRDUDE		=avrdude
EXTRA_DEFINES   =-DF_CPU=8000000UL # -DDEBUG for additional messages on UART line to ESP

ifneq (DEBUG,)
	EXTRA_DEFINES += -DDEBUG
endif

INCLUDES=
CFLAGS=-ffunction-sections -fdata-sections $(INCLUDES) $(EXTRA_DEFINES)
LD_FLAGS=-Wl,--gc-sections

COMPILE=avr-gcc -std=c99 -Wall -Os $(CFLAGS) -mmcu=$(DEVICE)
COMPILE_CPP=avr-g++ -Wall -Os $(CFLAGS) -mmcu=$(DEVICE)

LINK=avr-g++ -Wall -Os -mmcu=$(DEVICE) $(LD_FLAGS)

OBJECTS=main.o timer.o

all: main.hex

.cpp.o:
	$(COMPILE_CPP) -c $< -o $@

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -c $< -o $@

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex

main.elf: $(OBJECTS)
	$(LINK) $(LD_FLAGS) -o main.elf $(OBJECTS)

flash: all
	$(AVRDUDE) $(AVRDUDE_OPTS) -p $(DEVICE) -c $(PROGRAMMER) -U flash:w:main.hex:i

clean:
	rm -f main.hex main.elf $(OBJECTS)
