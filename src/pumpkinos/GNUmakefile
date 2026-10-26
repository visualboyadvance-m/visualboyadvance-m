# VBA-M PumpkinOS port.
#
# Builds the emulator core with the same defines as the libretro port plus
# the PalmOS glue in this directory into VBAM.dlib, wraps it into VBAM.prc
# with pilrc/prcbuild from the PumpkinOS tree, and installs the PRC into
# the PumpkinOS VFS (vfs/app_install), where it is deployed at next boot.
#
# Usage:
#   make PUMPKIN=/path/to/PumpkinOS        (default below)
#   make UNAME_S=msdosdjgpp                 (MS-DOS: VBAM.dlib is a DXE3
#                                            module, see the link rule)

PUMPKIN ?= ${HOME}/Downloads/PumpkinOS
VBAM_SRC := $(abspath ..)

PROGRAM = VBAM
APPNAME = VBAM
APPID   = VBAM

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# The SYS_* triple must match the one baked into $(PUMPKIN)/bin/libpumpkin:
# at runtime the app's native code is loaded from the PRC resource
# 'dlib' id = SYS_OS*64 + SYS_CPU*8 + SYS_SIZE.
ifeq ($(UNAME_S),msdosdjgpp)
SYS_OS ?= 7
else
ifeq ($(UNAME_S),msys2)
SYS_OS ?= 2
else
ifeq ($(UNAME_S),Android)
SYS_OS=4
else
SYS_OS ?= 1
endif
endif
endif

ifeq ($(findstring arm,$(UNAME_M)),arm)
SYS_CPU ?= 1
else ifeq ($(UNAME_M),aarch64)
SYS_CPU ?= 1
else ifeq ($(UNAME_M),riscv64)
SYS_CPU ?= 3
else
SYS_CPU ?= 2
endif

ifeq ($(UNAME_M),i686)
SYS_SIZE ?= 1
else ifeq ($(findstring arm,$(UNAME_M)),arm)
SYS_SIZE ?= 1
else
SYS_SIZE ?= 2
endif

SYS_ENDIAN ?= 1

DLIBID := $(shell expr $(SYS_OS) \* 64 + $(SYS_CPU) \* 8 + $(SYS_SIZE))
DLIB := $(shell printf "dlib%04x.bin" $(DLIBID))

PILRC = $(PUMPKIN)/tools/pilrc -q -allowLargeRes
PRCBUILD = $(PUMPKIN)/tools/prcbuild

ifeq ($(UNAME_S),Darwin)
CXX = clang++ -arch x86_64 -arch arm64 -mmacosx-version-min=10.15
LINKFLAGS = -dynamiclib
PICFLAG = -fPIC
OSDEFS = -DDARWIN -Wno-invalid-pp-token -Wno-string-plus-int -Wno-typedef-redefinition
else ifeq ($(UNAME_S),Android)
ifeq ($(NDK),)
$(error You must define the NDK environment variable)
endif
ifeq ($(UNAME_M),i686)
ANDROID_ARCH=--target=i686-none-linux-androideabi26
else ifeq ($(UNAME_M),x86_64)
ANDROID_ARCH=--target=x86_64-none-linux-android26
else ifeq ($(UNAME_M),riscv64)
ANDROID_ARCH=--target=riscv64-none-linux-android35
else
ifeq ($(BITS),32)
ANDROID_ARCH=--target=armv7-none-linux-androideabi26 -march=armv7-a -mthumb
else
ANDROID_ARCH=--target=aarch64-none-linux-android26
endif
endif
OSLDEFS=$(ANDROID_ARCH) --gcc-toolchain=$(NDK) --sysroot=$(NDK)/sysroot -fPIC -Wl,--exclude-libs,libgcc.a -Wl,--exclude-libs,libgcc_real.a -Wl,--exclude-libs,libatomic.a -static-libstdc++ -Wl,--build-id -Wl,--fatal-warnings -Wl,--exclude-libs,libunwind.a -Wl,--no-undefined -Qunused-arguments
EXTLIBS=-llog -latomic
SOEXT=.so
LUAPLAT=linux
OS=Android
OSDEFS=$(ANDROID_ARCH) --gcc-toolchain=$(NDK) --sysroot=$(NDK)/sysroot -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -DANDROID -DSOEXT=\"$(SOEXT)\"
CC=$(NDK)/bin/clang
CXX = $(NDK)/bin/clang++
LINKFLAGS = -shared $(OSLDEFS)
PICFLAG = -fPIC
else ifeq ($(UNAME_S),msdosdjgpp)
CXX = i686-pc-msdosdjgpp-g++
# DJGPP has no position-independent code (-fPIC is "ignored" with a notice)
PICFLAG =
OSDEFS = -DDOS -DW32_HAVE_STRUCT_TIMESPEC=1 -D__pthread_write=write -march=i586 -mtune=i586
# The module is linked by dxe3gen. Invoked as "dxegen" the same program runs
# in DXE1 mode (one exported symbol, no unresolved symbols), so it has to be
# called under its own name: when only dxegen is installed, obj/dxe3gen is a
# link to it.
DXE3GEN := $(shell command -v i686-pc-msdosdjgpp-dxe3gen dxe3gen 2>/dev/null | head -n 1)
ifeq ($(DXE3GEN),)
DXE3GEN = $(OBJDIR)/dxe3gen
DXE3GEN_DEP = $(OBJDIR)/dxe3gen
endif
else
CXX = g++
LINKFLAGS = -shared
PICFLAG = -fPIC
OSDEFS = -DLINUX
endif

# Core defines, mirroring src/libretro/CMakeLists.txt. __LIBRETRO__ is
# load-bearing: it selects the in-memory savestate API and the packed
# g_pix scanline stride this frontend relies on.
COREDEFS = -D__LIBRETRO__ -DFINAL_VERSION -DC_CORE -DNO_LINK -DNO_DEBUGGER \
           -DNO_PNG -DTILED_RENDERING -DAFFINE_BG_UNIFIED -DFRONTEND_SUPPORTS_RGB565

PUMPDEFS = -DSYS_CPU=$(SYS_CPU) -DSYS_SIZE=$(SYS_SIZE) -DSYS_OS=$(SYS_OS) \
           -DSYS_ENDIAN=$(SYS_ENDIAN) -DUNIX

PALMOS = $(PUMPKIN)/src/PalmOS
INCLUDES = -I. -I$(VBAM_SRC) \
           -I$(PUMPKIN)/src/libpit -I$(PUMPKIN)/src/libpumpkin -I$(PUMPKIN)/src/font \
           -I$(PALMOS) -I$(PALMOS)/Core -I$(PALMOS)/Core/System -I$(PALMOS)/Dynamic \
           -I$(PALMOS)/Core/UI -I$(PALMOS)/Libraries -I$(PALMOS)/Libraries/PalmOSGlue \
           -I$(PALMOS)/Core/Hardware -I$(PALMOS)/Extensions/ExpansionMgr

OPT ?= -O2
CXXFLAGS = -std=gnu++17 -Wall -Wno-unknown-pragmas -Wno-multichar -fsigned-char $(PICFLAG) \
           $(OPT) $(COREDEFS) $(PUMPDEFS) $(OSDEFS) $(INCLUDES)

GLUE_SRCS = vbam_pumpkin.cpp sound_pumpkin.cpp

CORE_SRCS = \
    core/base/internal/file_util_internal.cpp \
    core/base/file_util_common.cpp \
    core/base/file_util_libretro.cpp \
    core/base/null_sound_driver.cpp \
    core/base/script_hooks.cpp \
    core/apu/Gb_Oscs.cpp \
    core/apu/Gb_Apu_State.cpp \
    core/apu/Blip_Buffer.cpp \
    core/apu/Multi_Buffer.cpp \
    core/apu/Effects_Buffer.cpp \
    core/apu/Gb_Apu.cpp \
    core/gba/gbaScheduler.cpp \
    core/gba/gbaMgbaLog.cpp \
    core/gba/gba.cpp \
    core/gba/gbaCheats.cpp \
    core/gba/gbaCpuArm.cpp \
    core/gba/gbaCpuThumb.cpp \
    core/gba/gbaEeprom.cpp \
    core/gba/gbaFlash.cpp \
    core/gba/gbaGfx.cpp \
    core/gba/gbaGlobals.cpp \
    core/gba/gbaMode0.cpp \
    core/gba/gbaMode1.cpp \
    core/gba/gbaMode2.cpp \
    core/gba/gbaMode3.cpp \
    core/gba/gbaMode4.cpp \
    core/gba/gbaMode5.cpp \
    core/gba/gbaPrint.cpp \
    core/gba/gbaRtc.cpp \
    core/gba/gbaSound.cpp \
    core/gba/internal/gbaBios.cpp \
    core/gba/internal/gbaEreader.cpp \
    core/gba/internal/gbaSram.cpp \
    core/gb/gb.cpp \
    core/gb/gbPowerOn.cpp \
    core/gb/gbCartData.cpp \
    core/gb/gbCheats.cpp \
    core/gb/gbGfx.cpp \
    core/gb/gbGlobals.cpp \
    core/gb/gbMemory.cpp \
    core/gb/gbSGB.cpp \
    core/gb/gbSound.cpp

OBJDIR = obj
OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(notdir $(GLUE_SRCS) $(CORE_SRCS)))

vpath %.cpp . $(sort $(dir $(addprefix $(VBAM_SRC)/,$(CORE_SRCS))))

all: $(PROGRAM).prc
ifeq ($(UNAME_S),msdosdjgpp)
all: dxesyms.c
endif

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(OBJDIR)
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

# MS-DOS: the dlib is a DXE3 module holding the emulator objects only. It
# exports PilotMain and imports (-U) everything else: the PalmOS API, libpit,
# libc, libgcc and libstdc++ come from pumpkin.exe at load time, which must
# export them with dlregsym() (dxesyms.c below is that table). Linking the
# PumpkinOS libraries into the module would give it a second copy of the OS
# state, and Watt-32's assembly objects (CODE/DATA sections, which dxe.ld
# does not fold into .text) are what made dxe3gen refuse the output as
# having more than one section.
$(OBJDIR)/dxe3gen:
	@mkdir -p $(OBJDIR)
	@test -n "$$(command -v dxegen)" || { echo "dxe3gen (or dxegen) not found"; exit 1; }
	@ln -sf "$$(command -v dxegen)" $@

$(PROGRAM).dlib: $(OBJS) $(DXE3GEN_DEP)
	@echo Linking $(PROGRAM).dlib
ifeq ($(UNAME_S),msdosdjgpp)
	@$(DXE3GEN) -o $@ -U -E _PilotMain -D "VBA-M" $(OBJS)
	@test -f $@ || { echo "$(DXE3GEN) failed (it exits 0 when its link step fails)"; exit 1; }
else
	@$(CXX) $(LINKFLAGS) -o $@ $(OBJS) -L$(PUMPKIN)/bin -lpumpkin -lpit
endif
ifeq ($(UNAME_S),Darwin)
	@codesign -f -s "Developer ID Application" $@ 2>/dev/null || codesign -f -s - $@
endif

# the symbols pumpkin.exe has to export for the module (see dxe3res.py). The
# table and the libstdc++ Pth shims (the host's <thread> objects, pulled in
# by the table's std::this_thread::sleep_for import, reference
# __pthread_sleep/usleep/detach; see src/sdl/dos-libstdcxx-shims.c) are
# installed into $(PUMPKIN)/src/msdos, whose GNUmakefile compiles whatever
# dxe*.c it finds there into pumpkin.exe. Relink pumpkin.exe after this
# (make OSNAME=msdosdjgpp UNAME_M=i686 -f GNUMakefile -C $(PUMPKIN)/src/msdos),
# or dlopen fails with "unresolved symbol" on the module's first import.
dxesyms.c: $(PROGRAM).dlib dxe3res.py
	@echo Generating $@
	@python3 dxe3res.py -o $@ $<
	@echo Installing $@ and dxeshims.c into $(PUMPKIN)/src/msdos
	@mkdir -p $(PUMPKIN)/src/msdos
	@cp $@ $(PUMPKIN)/src/msdos/dxesyms.c
	@cp $(VBAM_SRC)/sdl/dos-libstdcxx-shims.c $(PUMPKIN)/src/msdos/dxeshims.c

res.flag: resource.rcp resource.h vbam.bmp
	@echo Compiling resources
	@mkdir -p resources
	@$(PILRC) resource.rcp resources
	@touch res.flag

$(PROGRAM).prc: $(PROGRAM).dlib res.flag
	@echo Building $(PROGRAM).prc
	@cp $(PROGRAM).dlib $(DLIB)
	@$(PRCBUILD) -f $(PROGRAM).prc -t appl -c $(APPID) -n "$(APPNAME)" $(DLIB) resources/*
	@rm -f $(DLIB)
	@echo Installing $(PROGRAM).prc into $(PUMPKIN)/vfs/app_install
	@mkdir -p $(PUMPKIN)/vfs/app_install
	@cp $(PROGRAM).prc $(PUMPKIN)/vfs/app_install/

clean:
	rm -rf $(OBJDIR) resources res.flag $(PROGRAM).dlib $(PROGRAM).prc $(DLIB) dxesyms.c

.PHONY: all clean
