TILED_RENDERING = 0

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
endif
endif

TARGET_NAME = vbam

ifeq ($(platform), unix)
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   SHARED := -shared
	TILED_RENDERING = 1
else ifeq ($(platform), osx)
   TARGET := $(TARGET_NAME)_libretro.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
else
   TARGET := $(TARGET_NAME)_libretro.dll
   LDFLAGS += -Wl,-no-undefined -Wl,--version-script=link.T
   CC = gcc
   CXX = g++
   SHARED := -shared -static-libgcc -static-libstdc++
endif

ifeq ($(TILED_RENDERING), 1)
VBA_DEFINES += -DTILED_RENDERING
endif

VBA_DIR := ../

VBA_SRC_DIRS := $(VBA_DIR)/gba $(VBA_DIR)/apu 

VBA_CXXSRCS := $(foreach dir,$(VBA_SRC_DIRS),$(wildcard $(dir)/*.cpp))
VBA_CXXOBJ := $(VBA_CXXSRCS:.cpp=.o) ../common/Patch.o
VBA_CSRCS := $(foreach dir,$(VBA_SRC_DIRS),$(wildcard $(dir)/*.c))
VBA_COBJ := $(VBA_CSRCS:.c=.o)
UTIL_SOURCES := $(wildcard ../common/utils/zlib/*.c)
UTIL_OBJS := $(UTIL_SOURCES:.c=.o)

OBJS := $(VBA_COBJ) $(VBA_CXXOBJ) $(UTIL_OBJS) libretro.o UtilRetro.o SoundRetro.o scrc32.o

VBA_DEFINES += -D__LIBRETRO__ -DFINAL_VERSION -DC_CORE -DUSE_GBA_ONLY -DNO_LINK
VBA_DEFINES += -DFRONTEND_SUPPORTS_RGB565

CFLAGS += -O3 -std=gnu99 $(fpic) $(VBA_DEFINES) -I../common/utils/zlib
CXXFLAGS += -O3 $(fpic) $(VBA_DEFINES) -I../common/utils/zlib


INCDIRS := -I$(VBA_DIR)
LIBS :=

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(SHARED) $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCDIRS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(INCDIRS)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean


