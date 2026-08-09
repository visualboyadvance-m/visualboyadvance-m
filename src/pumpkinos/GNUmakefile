# VBA-M PumpkinOS port.
#
# Builds the emulator core with the same defines as the libretro port plus
# the PalmOS glue in this directory into VBAM.dlib, wraps it into VBAM.prc
# with pilrc/prcbuild from the PumpkinOS tree, and installs the PRC into
# the PumpkinOS VFS (vfs/app_install), where it is deployed at next boot.
#
# Usage:
#   make PUMPKIN=/path/to/PumpkinOS        (default below)

PUMPKIN ?= /Users/andyvand/Downloads/PumpkinOS
VBAM_SRC := $(abspath ..)

PROGRAM = VBAM
APPNAME = VBAM
APPID   = VBAM

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# The SYS_* triple must match the one baked into $(PUMPKIN)/bin/libpumpkin:
# at runtime the app's native code is loaded from the PRC resource
# 'dlib' id = SYS_OS*64 + SYS_CPU*8 + SYS_SIZE.
SYS_OS ?= 1
ifeq ($(findstring arm,$(UNAME_M)),arm)
SYS_CPU ?= 1
else ifeq ($(UNAME_M),aarch64)
SYS_CPU ?= 1
else
SYS_CPU ?= 2
endif
SYS_SIZE ?= 2
SYS_ENDIAN ?= 1
DLIBID := $(shell expr $(SYS_OS) \* 64 + $(SYS_CPU) \* 8 + $(SYS_SIZE))
DLIB := $(shell printf "dlib%04x.bin" $(DLIBID))

PILRC = $(PUMPKIN)/tools/pilrc -q -allowLargeRes
PRCBUILD = $(PUMPKIN)/tools/prcbuild

ifeq ($(UNAME_S),Darwin)
CXX = clang++ -arch x86_64 -arch arm64 -mmacosx-version-min=10.15
LINKFLAGS = -dynamiclib
OSDEFS = -DDARWIN -Wno-invalid-pp-token -Wno-string-plus-int -Wno-typedef-redefinition
else
CXX = g++
LINKFLAGS = -shared
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
CXXFLAGS = -std=gnu++17 -Wall -Wno-unknown-pragmas -Wno-multichar -fsigned-char -fPIC \
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

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(OBJDIR)
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

$(PROGRAM).dlib: $(OBJS)
	@echo Linking $(PROGRAM).dlib
	@$(CXX) $(LINKFLAGS) -o $@ $(OBJS) -L$(PUMPKIN)/bin -lpumpkin -lpit
ifeq ($(UNAME_S),Darwin)
	@codesign -f -s "Developer ID Application" $@ 2>/dev/null || codesign -f -s - $@
endif

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
	rm -rf $(OBJDIR) resources res.flag $(PROGRAM).dlib $(PROGRAM).prc $(DLIB)

.PHONY: all clean
