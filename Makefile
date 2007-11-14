CC=gcc
CPPC=g++
CFLAGS=-W -Wall -Wno-unused -DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DFINAL_VERSION -DC_CORE -DSDL -DSYSCONFDIR="home"
CXXFLAGS=${CFLAGS}
ASM=nasm
ASMFLAGS=-w-orphan-labels -f elf -DELF -O1 -Isrc/
LFLAGS=-lz -lpng -lSDL -s

MAINDIR=src
SDLDIR=src/sdl
DMGDIR=src/gb
RESAMPLEDIR=src/libresample-0.1.3

MAINOBJ=${MAINDIR}/2xSaI.o ${MAINDIR}/admame.o ${MAINDIR}/agbprint.o ${MAINDIR}/armdis.o \
${MAINDIR}/bilinear.o ${MAINDIR}/bios.o ${MAINDIR}/Cheats.o ${MAINDIR}/CheatSearch.o \
${MAINDIR}/EEprom.o ${MAINDIR}/elf.o ${MAINDIR}/Flash.o ${MAINDIR}/GBA.o \
${MAINDIR}/gbafilter.o ${MAINDIR}/Gfx.o ${MAINDIR}/Globals.o ${MAINDIR}/hq2x.o \
${MAINDIR}/hq3x32.o ${MAINDIR}/hq4x.o ${MAINDIR}/hq_shared32.o ${MAINDIR}/interframe.o \
${MAINDIR}/lq3x.o ${MAINDIR}/lq4x.o ${MAINDIR}/Mode0.o \
${MAINDIR}/Mode1.o ${MAINDIR}/Mode2.o ${MAINDIR}/Mode3.o ${MAINDIR}/Mode4.o \
${MAINDIR}/Mode5.o ${MAINDIR}/motionblur.o ${MAINDIR}/pixel.o ${MAINDIR}/portable.o \
${MAINDIR}/remote.o ${MAINDIR}/RTC.o ${MAINDIR}/scanline.o ${MAINDIR}/simpleFilter.o \
${MAINDIR}/snd_interp.o ${MAINDIR}/Sound.o ${MAINDIR}/Sram.o ${MAINDIR}/Text.o \
${MAINDIR}/unzip.o ${MAINDIR}/Util.o ${MAINDIR}/exprNode.o ${MAINDIR}/getopt.o \
${MAINDIR}/getopt1.o ${MAINDIR}/memgzio.o ${MAINDIR}/hq3x_16.o ${MAINDIR}/hq3x_32.o \
${MAINDIR}/hq4x_16.o ${MAINDIR}/hq4x_32.o ${MAINDIR}/expr-lex.o ${MAINDIR}/expr.o

DMGOBJ=${DMGDIR}/GB.o ${DMGDIR}/gbCheats.o ${DMGDIR}/gbDis.o ${DMGDIR}/gbGfx.o \
${DMGDIR}/gbGlobals.o ${DMGDIR}/gbMemory.o ${DMGDIR}/gbPrinter.o ${DMGDIR}/gbSGB.o \
${DMGDIR}/gbSound.o

SDLOBJ=${SDLDIR}/debugger.o ${SDLDIR}/SDL.o ${SDLDIR}/dummy.o

OBJECTS=${MAINOBJ} ${DMGOBJ} ${SDLOBJ}
LIB=${RESAMPLEDIR}/libresample.a

.SUFFIXES: .c .cpp

%.o: %.c
	${CC} ${CFLAGS} -o $@ -c $<

%.o: %.cpp
	${CPPC} ${CXXFLAGS} -o $@ -c $<

%.o: %.asm
	${ASM} ${ASMFLAGS} -o $@ $<

ALL: vba

vba: ${OBJECTS} ${LIB}
	$(CPPC) -o $@ ${OBJECTS} ${LIB} ${LFLAGS}

${RESAMPLEDIR}/libresample.a:
	make -C ${RESAMPLEDIR} -f Make

clean:
	rm -f vba ${OBJECTS} ${LIB}
