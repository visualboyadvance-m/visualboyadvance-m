CC=gcc
CPPC=g++
CFLAGS=-W -Wall -Wno-unused -DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DFINAL_VERSION -DC_CORE -DSDL -DSYSCONFDIR="home" -DUSE_OPENGL
CXXFLAGS=${CFLAGS}
ASM=nasm
ASMFLAGS=-w-orphan-labels -f elf -DELF -O1 -Isrc/
LFLAGS=-lz -lpng -lopengl32 `sdl-config --libs`
STRIP=strip -s
DEL=rm -f
OE=.o
OUT=vba

ifeq ($(PLATFORM),win)
  ASMFLAGS=-w-orphan-labels -f win32 -O1 -Isrc/
  LFLAGS=-lz -lpng -lSDL -lwsock32 -lopengl32
  DELETECOMMAND = del
  OE=.obj
  OUT=vba.exe
endif

ifeq ($(PLATFORM),win-cross)
  CC=i586-mingw32-gcc
  CPPC=i586-mingw32-g++
  ASMFLAGS=-w-orphan-labels -f win32 -O1 -Isrc/
  LFLAGS=-lz -lpng -lSDL -lwsock32 -lopengl32
  STRIP=i586-mingw32-strip -s
  OE=.obj
  OUT=vba.exe
endif

MAINDIR=src
SDLDIR=src/sdl
DMGDIR=src/gb
RESAMPLEDIR=src/libresample-0.1.3/src


ASMOBJ=${MAINDIR}/hq3x_16${OE} ${MAINDIR}/hq3x_32${OE} ${MAINDIR}/hq4x_16${OE} ${MAINDIR}/hq4x_32${OE} \
${MAINDIR}/hq3x32${OE}

CALTERNOBJ=${MAINDIR}/hq3x16c${OE} ${MAINDIR}/hq3x32c${OE} ${MAINDIR}/hq4x16c${OE} ${MAINDIR}/hq4x32c${OE} \
${MAINDIR}/hq_shared32${OE}

MAINOBJ=${MAINDIR}/2xSaI${OE} ${MAINDIR}/admame${OE} ${MAINDIR}/agbprint${OE} ${MAINDIR}/armdis${OE} \
${MAINDIR}/bilinear${OE} ${MAINDIR}/bios${OE} ${MAINDIR}/Cheats${OE} ${MAINDIR}/CheatSearch${OE} \
${MAINDIR}/EEprom${OE} ${MAINDIR}/elf${OE} ${MAINDIR}/Flash${OE} ${MAINDIR}/GBA${OE} \
${MAINDIR}/gbafilter${OE} ${MAINDIR}/Gfx${OE} ${MAINDIR}/Globals${OE} ${MAINDIR}/interframe${OE} \
${MAINDIR}/hq2x${OE}  ${MAINDIR}/Mode0${OE} \
${MAINDIR}/Mode1${OE} ${MAINDIR}/Mode2${OE} ${MAINDIR}/Mode3${OE} ${MAINDIR}/Mode4${OE} \
${MAINDIR}/Mode5${OE} ${MAINDIR}/motionblur${OE} ${MAINDIR}/pixel${OE} ${MAINDIR}/portable${OE} \
${MAINDIR}/remote${OE} ${MAINDIR}/RTC${OE} ${MAINDIR}/scanline${OE} ${MAINDIR}/simpleFilter${OE} \
${MAINDIR}/snd_interp${OE} ${MAINDIR}/Sound${OE} ${MAINDIR}/Sram${OE} ${MAINDIR}/Text${OE} \
${MAINDIR}/unzip${OE} ${MAINDIR}/Util${OE} ${MAINDIR}/exprNode${OE} ${MAINDIR}/getopt${OE} \
${MAINDIR}/getopt1${OE} ${MAINDIR}/memgzio${OE} ${MAINDIR}/expr-lex${OE} ${MAINDIR}/expr${OE}

DMGOBJ=${DMGDIR}/GB${OE} ${DMGDIR}/gbCheats${OE} ${DMGDIR}/gbDis${OE} ${DMGDIR}/gbGfx${OE} \
${DMGDIR}/gbGlobals${OE} ${DMGDIR}/gbMemory${OE} ${DMGDIR}/gbPrinter${OE} ${DMGDIR}/gbSGB${OE} \
${DMGDIR}/gbSound${OE}

SDLOBJ=${SDLDIR}/debugger${OE} ${SDLDIR}/SDL${OE} ${SDLDIR}/dummy${OE}

OBJECTS=${MAINOBJ} ${DMGOBJ} ${SDLOBJ}
LIB=${RESAMPLEDIR}/filterkit${OE} ${RESAMPLEDIR}/resample${OE} ${RESAMPLEDIR}/resamplesubs${OE}

ifeq ($(USEASM),yes)
OBJECTS+=${ASMOBJ}
else
OBJECTS+=${CALTERNOBJ}
endif


.SUFFIXES: .c .cpp .asm

%${OE}: %.c
	${CC} ${CFLAGS} -o $@ -c $<

%${OE}: %.cpp
	${CPPC} ${CXXFLAGS} -o $@ -c $<

%${OE}: %.asm
	${ASM} ${ASMFLAGS} -o $@ $<

ALL: ${OUT}

${OUT}: ${OBJECTS} ${LIB}
	$(CPPC) -o $@ ${OBJECTS} ${LIB} ${LFLAGS}
	$(STRIP) $@

clean:
	$(DEL) ${OUT} ${OBJECTS} ${LIB}
