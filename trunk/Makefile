MACHINE= $(shell uname -s)
CC=gcc
CPPC=g++
CFLAGS=-W -Wall -Wno-unused -O3 -DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DFINAL_VERSION -DBKPT_SUPPORT -DSDL -DSYSCONFDIR="home" -DUSE_OPENGL -DC_CORE
CXXFLAGS=${CFLAGS}
ASM=nasm
ASMFLAGS=-w-orphan-labels -f elf -DELF -O1 -Isrc/hq/asm/
LFLAGS=-lz -lpng -lGL `sdl-config --libs`
STRIP=strip -s
DEL=rm -f
OE=.o
OUT=vba

ifeq ($(MACHINE),Darwin)
	LFLAGS=-lz -lpng -framework OpenGL `sdl-config --libs`
endif

ifeq ($(PLATFORM),win)
  ASMFLAGS=-w-orphan-labels -f win32 -O1 -Isrc/hq/asm/
  LFLAGS=-lz -lpng -lSDL -lwsock32 -lopengl32
  DELETECOMMAND = del
  OE=.obj
  OUT=vba.exe
endif

ifeq ($(PLATFORM),win-cross)
  CC=i586-mingw32-gcc
  CPPC=i586-mingw32-g++
  ASMFLAGS=-w-orphan-labels -f win32 -O1 -Isrc/hq/asm/
  LFLAGS=-lz -lpng -lSDL -lwsock32 -lopengl32
  STRIP=i586-mingw32-strip -s
  OE=.obj
  OUT=vba.exe
endif

MAINDIR=src
SDLDIR=src/sdl
DMGDIR=src/gb
GBAPUDIR=src/gb/gb_apu
FEXDIR=../dependencies/File_Extractor-0.4.3
HQCDIR=src/hq/c
HQASMDIR=src/hq/asm


ASMOBJ=${HQASMDIR}/hq3x_16${OE} ${HQASMDIR}/hq3x_32${OE} ${HQASMDIR}/hq4x_16${OE} \
${HQASMDIR}/hq4x_32${OE} ${HQASMDIR}/hq3x32${OE}

GBAPUOBJ=${GBAPUDIR}/Blip_Buffer${OE} ${GBAPUDIR}/Effects_Buffer${OE} ${GBAPUDIR}/Gb_Apu${OE} \
${GBAPUDIR}/Gb_Apu_State${OE} ${GBAPUDIR}/Gb_Oscs${OE} ${GBAPUDIR}/Multi_Buffer${OE}

CALTERNOBJ=${HQCDIR}/hq_implementation${OE}

MAINOBJ=${MAINDIR}/2xSaI${OE} ${MAINDIR}/admame${OE} ${MAINDIR}/agbprint${OE} ${MAINDIR}/armdis${OE} \
${MAINDIR}/bilinear${OE} ${MAINDIR}/bios${OE} ${MAINDIR}/Cheats${OE} ${MAINDIR}/CheatSearch${OE} \
${MAINDIR}/EEprom${OE} ${MAINDIR}/elf${OE} ${MAINDIR}/Flash${OE} ${MAINDIR}/GBA${OE} \
${MAINDIR}/gbafilter${OE} ${MAINDIR}/Gfx${OE} ${MAINDIR}/Globals${OE} ${MAINDIR}/interframe${OE} \
${MAINDIR}/hq2x${OE} ${MAINDIR}/GBA-thumb${OE} ${MAINDIR}/GBA-arm${OE} ${MAINDIR}/Mode0${OE} \
${MAINDIR}/Mode1${OE} ${MAINDIR}/Mode2${OE} ${MAINDIR}/Mode3${OE} ${MAINDIR}/Mode4${OE} \
${MAINDIR}/Mode5${OE} ${MAINDIR}/motionblur${OE} ${MAINDIR}/pixel${OE} \
${MAINDIR}/remote${OE} ${MAINDIR}/RTC${OE} ${MAINDIR}/scanline${OE} \
${MAINDIR}/Sound${OE} ${MAINDIR}/Sram${OE} ${MAINDIR}/Text${OE} ${MAINDIR}/Util${OE} \
${MAINDIR}/expr${OE} ${MAINDIR}/exprNode${OE} ${MAINDIR}/expr-lex${OE} \
${MAINDIR}/memgzio${OE} 

DMGOBJ=${DMGDIR}/GB${OE} ${DMGDIR}/gbCheats${OE} ${DMGDIR}/gbDis${OE} ${DMGDIR}/gbGfx${OE} \
${DMGDIR}/gbGlobals${OE} ${DMGDIR}/gbMemory${OE} ${DMGDIR}/gbPrinter${OE} ${DMGDIR}/gbSGB${OE} \
${DMGDIR}/gbSound${OE}

SDLOBJ=${SDLDIR}/debugger${OE} ${SDLDIR}/SDL${OE} ${SDLDIR}/dummy${OE} ${SDLDIR}/filters${OE}

OBJECTS=${MAINOBJ} ${DMGOBJ} ${SDLOBJ} ${GBAPUOBJ}

ifeq ($(USEASM),yes)
OBJECTS+=${ASMOBJ}
else
OBJECTS+=${CALTERNOBJ}
endif

ifeq ($(USEFEX),yes)
LFLAGS+=-L${FEXDIR} -lfex
else
OBJECTS+=${MAINDIR}/fex_mini${OE}
endif



.SUFFIXES: .c .cpp .asm

%${OE}: %.c
	${CC} ${CFLAGS} -o $@ -c $<

%${OE}: %.cpp
	${CPPC} ${CXXFLAGS} -o $@ -c $<

%${OE}: %.asm
	${ASM} ${ASMFLAGS} -o $@ $<

ALL: ${OUT}

${OUT}: ${OBJECTS} 
	$(CPPC) -o $@ ${OBJECTS} ${LFLAGS}
	$(STRIP) $@

clean:
	$(DEL) ${OUT} ${OBJECTS} 
