PICP_TTY=/dev/ttyUSB0
CHIP=16F873A
#CHIP=16F77
#CHIP=16F72
FREQ=24000000
SRCS=main.c lcd.c delay.c


main.hex: $(SRCS)
	picc -D_XTAL_FREQ=$(FREQ) --chip=$(CHIP) $(SRCS)

clean:
	rm -f *.hex
	rm -f *.rlf *.obj *.as *.lst *.sym *.sdb *.cof *.pre *.p1 funclist *.hxl

# erase whole chip, verify program mem is blank, the write program mem
pgm: main.hex
	sudo picp $(PICP_TTY) $(CHIP) -ef
	sudo picp $(PICP_TTY) $(CHIP) -bp
	sudo picp $(PICP_TTY) $(CHIP) -wp $<

# read config word
config:
	sudo picp $(PICP_TTY) $(CHIP) -rc
ver:
	sudo picp $(PICP_TTY) -v
read:
	sudo picp $(PICP_TTY) $(CHIP) -rp foo.hex
