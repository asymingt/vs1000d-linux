# Toolchain configuration
FEATURES     = 
COFF2BOOT    = ./coff2boot.bin
COFF2SPIBOOT = ./coff2spiboot.bin
VCCFLAGS     = -P130 -g -O -fsmall-code $(FEATURES)
RM           = rm -f
DEBUG        = -g
BINDIR       = ./toolchain/bin
INCDIR       = ./toolchain/include
LIBDIR       = ./toolchain/lib

# Set up search paths
INCDIRS      = -I . -I $(INCDIR)
LIBDIRS      = -L . -L $(LIBDIR) -L $(INCDIR)
LIBS         = -lc -ldev1000
VCC          = $(BINDIR)/vcc
VSA          = $(BINDIR)/vsa
VSLINK       = $(BINDIR)/vslink
VSEMU        = $(BINDIR)/vs3emu
VCCFLAGS     = -P130 $(DEBUG) -O -fsmall-code $(FEATURES)
ASMFLAGS     = -D ASM $(FEATURES)

# Default make target
all: coff2spiboot.bin coff2boot.bin flasher.bin uartcontrol.bin boot.img

.c.o:
	$(VCC) $(VCCFLAGS) $(INCDIRS) -o $@ $<

.s.o:
	$(VSA) $(ASMFLAGS) $(INCDIRS) -o $@ $<

coff2spiboot.bin: coff2spiboot.c
	gcc -Wall -o $@ $<

coff2boot.bin: coff2boot.c
	gcc -Wall -o $@ $<

.bin.img: Makefile
	$(COFF2BOOT) -c 2 -w 50 -x 0x50 $< $@

.bin.spi: Makefile
	$(COFF2SPIBOOT) -x 0x50 $< $@

.bin.prg: Makefile
	$(COFF2BOOT) -n -x 0x50 $< $@

.o.bin:
	$(VSLINK) -k -m mem_desc.vs1000 -o $@ $(INCDIR)/c-nand.o $< $(INCDIR)/rom1000.o $(LIBDIRS) $(LIBS)

flasher.bin: $(INCDIR)/c-restart.o flasher.o $(INCDIR)/rom1000.o
	$(VSLINK) -k -m mem_desc.vs1000 -o $@ $(INCDIR)/c-restart.o flasher.o $(INCDIR)/rom1000.o $(LIBDIRS) $(LIBS)

uartcontrol.bin: uartcontrol.o uart.o
	$(VSLINK) -k -m mem_desc.vs1000+uart -o $@ $(INCDIR)/c-nand.o uartcontrol.o uart.o $(INCDIR)/rom1000.o $(LIBDIRS) $(LIBS)

boot.img: uartcontrol.bin
	$(COFF2BOOT) -t 3 -b 8 -s 19 -w 90 -x 0x50 $< boot.img

flash:
	$(VSEMU) -chip vs1000 -s 115200 -l flasher.bin -c run.cmd

clean:
	$(RM) *.o *.a *.bin *.img *~
