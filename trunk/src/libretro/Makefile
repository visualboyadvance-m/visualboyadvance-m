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
   arch = intel
ifeq ($(shell uname -a),)
   arch = ppc
endif
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
   arch = intel
ifeq ($(shell uname -a),)
   arch = ppc
endif

ifneq ($(arch),ppc)
   SHARED += -DLSB_FIRST
endif
   OSXVER = `sw_vers -productVersion | cut -d. -f 2`
   OSX_LT_MAVERICKS = `(( $(OSXVER) <= 9)) && echo "YES"`
ifeq ($(OSX_LT_MAVERICKS),"YES")
   fpic += -mmacosx-version-min=10.5
endif
else ifeq ($(platform), ios)
   TARGET := $(TARGET_NAME)_libretro_ios.dylib
   fpic := -fPIC
   SHARED := -dynamiclib -DLSB_FIRST

ifeq ($(IOSSDK),)
   IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
endif

   CC = clang -arch armv7 -isysroot $(IOSSDK)
   CXX = clang++ -arch armv7 -isysroot $(IOSSDK)
   OSXVER = `sw_vers -productVersion | cut -d. -f 2`
   OSX_LT_MAVERICKS = `(( $(OSXVER) <= 9)) && echo "YES"`
ifeq ($(OSX_LT_MAVERICKS),"YES")
   CC += -miphoneos-version-min=5.0
   CXX += -miphoneos-version-min=5.0
   SHARED += -miphoneos-version-min=5.0
endif
	TILED_RENDERING = 1
else ifeq ($(platform), theos_ios)
DEPLOYMENT_IOSVERSION = 5.0
TARGET = iphone:latest:$(DEPLOYMENT_IOSVERSION)
ARCHS = armv7 armv7s
TARGET_IPHONEOS_DEPLOYMENT_VERSION=$(DEPLOYMENT_IOSVERSION)
THEOS_BUILD_DIR := objs
include $(THEOS)/makefiles/common.mk

LIBRARY_NAME = $(TARGET_NAME)_libretro_ios

   ENDIANNESS_DEFINES = -DLSB_FIRST
	TILED_RENDERING=1
else ifeq ($(platform), qnx)
   TARGET := $(TARGET_NAME)_libretro_qnx.so
   fpic := -fPIC
   SHARED := -shared
	TILED_RENDERING = 1
	CC = qcc -Vgcc_ntoarmv7le
	CXX = QCC -Vgcc_ntoarmv7le_cpp
	AR = QCC -Vgcc_ntoarmv7le
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

CORE_DIR := ..

include Makefile.common

OBJS := $(SOURCES_CXX:.cpp=.o)

VBA_DEFINES += -D__LIBRETRO__ -DFINAL_VERSION -DC_CORE -DUSE_GBA_ONLY -DNO_LINK
VBA_DEFINES += -DFRONTEND_SUPPORTS_RGB565

ifeq ($(DEBUG), 1)
   CFLAGS += -g
   CXXFLAGS += -g
else
   CFLAGS += -O3 -DNDEBUG
   CXXFLAGS += -O3 -DNDEBUG
endif

CFLAGS += $(fpic) $(VBA_DEFINES)
CXXFLAGS += $(fpic) $(VBA_DEFINES)

LIBS :=

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(INCFLAGS)

ifeq ($(platform), theos_ios)
COMMON_FLAGS := -DIOS $(COMMON_DEFINES) $(INCFLAGS) -I$(THEOS_INCLUDE_PATH) -Wno-error
$(LIBRARY_NAME)_CFLAGS += $(CFLAGS) $(COMMON_FLAGS)
$(LIBRARY_NAME)_CXXFLAGS += $(CXXFLAGS) $(COMMON_FLAGS)
${LIBRARY_NAME}_FILES = $(SOURCES_CXX) $(SOURCES_C)
include $(THEOS_MAKE_PATH)/library.mk
else
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(SHARED) $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean
endif
