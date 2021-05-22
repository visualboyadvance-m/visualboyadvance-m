DEBUG=0
TILED_RENDERING=0
STATIC_LINKING=0
FRONTEND_SUPPORTS_RGB565=1
NO_LINK=1

SPACE :=
SPACE := $(SPACE) $(SPACE)
BACKSLASH :=
BACKSLASH := \$(BACKSLASH)
filter_out1 = $(filter-out $(firstword $1),$1)
filter_out2 = $(call filter_out1,$(call filter_out1,$1))

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
	platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	platform = osx
	arch = intel
ifeq ($(shell uname -p),arm)
	arch = arm
endif
ifeq ($(shell uname -p),powerpc)
	arch = ppc
endif
else ifneq ($(findstring win,$(shell uname -a)),)
	platform = win
endif
endif

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
EXE_EXT = .exe
	system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	system_platform = osx
	arch = intel
ifeq ($(shell uname -p),arm)
	arch = arm
endif
ifeq ($(shell uname -p),powerpc)
	arch = ppc
endif
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	system_platform = win
endif

CORE_DIR     := ..
LIBRETRO_DIR := $(CORE_DIR)/libretro
TARGET_NAME  := vbam

VBAM_VERSION   := $(shell sed -En 's/.*\[([0-9]+[^]]+).*/\1/p; T; q' ../../CHANGELOG.md 2>/dev/null)
TAG_COMMIT     := $(shell git rev-list -n 1 v$(VBAM_VERSION) --abbrev-commit 2>/dev/null)
CURRENT_COMMIT := $(shell git rev-parse --short HEAD 2>/dev/null)

CXXFLAGS += -DVBAM_VERSION=\"$(VBAM_VERSION)\"
ifneq ($(CURRENT_COMMIT),$(TAG_COMMIT))
	CXXFLAGS += -DGIT_COMMIT=\"$(CURRENT_COMMIT)\"
endif

# UNIX
ifneq (,$(findstring unix,$(platform)))
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	SHARED := -shared -Wl,-version-script=$(LIBRETRO_DIR)/link.T -Wl,-no-undefined
	TILED_RENDERING=1

# Classic Platforms ####################
# Platform affix = classic_<ISA>_<µARCH>
# Help at https://modmyclassic.com/comp

# (armv7 a7, hard point, neon based) ###
# NESC, SNESC, C64 mini
else ifeq ($(platform), classic_armv7_a7)
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=$(LIBRETRO_DIR)/link.T -Wl,--no-undefined -fPIC
	CFLAGS += -Ofast \
	-flto=4 -fwhole-program -fuse-linker-plugin \
	-fdata-sections -ffunction-sections -Wl,--gc-sections \
	-fno-stack-protector -fno-ident -fomit-frame-pointer \
	-falign-functions=1 -falign-jumps=1 -falign-loops=1 \
	-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-unroll-loops \
	-fmerge-all-constants -fno-math-errno \
	-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
	CXXFLAGS += $(CFLAGS)
	CPPFLAGS += $(CFLAGS)
	ASFLAGS += $(CFLAGS)
	HAVE_NEON=1
	ARCH = arm
	BUILTIN_GPU = neon
	USE_DYNAREC=1
	TILED_RENDERING=1
	ifeq ($(shell echo `$(CC) -dumpversion` "< 4.9" | bc -l), 1)
	  CFLAGS += -march=armv7-a
	else
	  CFLAGS += -march=armv7ve
	  # If gcc is 5.0 or later
	  ifeq ($(shell echo `$(CC) -dumpversion` ">= 5" | bc -l), 1)
	    LDFLAGS += -static-libgcc -static-libstdc++
	  endif
	endif

# (armv8 a35, hard point, neon based) ###
# PlayStation Classic
else ifeq ($(platform), classic_armv8_a35)
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	SHARED := -shared
	TILED_RENDERING=1
	CFLAGS += -Ofast \
	-flto=4 -fwhole-program -fuse-linker-plugin \
	-fdata-sections -ffunction-sections -Wl,--gc-sections \
	-fno-stack-protector -fno-ident -fomit-frame-pointer \
	-falign-functions=1 -falign-jumps=1 -falign-loops=1 \
	-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-unroll-loops \
	-fmerge-all-constants -fno-math-errno \
	-marm -mtune=cortex-a35 -mfpu=neon-fp-armv8 -mfloat-abi=hard
	CXXFLAGS += $(CFLAGS)
	HAVE_NEON=1
	ARCH = arm
	ifeq ($(shell echo `$(CC) -dumpversion` "< 4.9" | bc -l), 1)
	  CFLAGS += -march=armv8-a
	else
	  CFLAGS += -march=armv8-a
	  # If gcc is 5.0 or later
	  ifeq ($(shell echo `$(CC) -dumpversion` ">= 5" | bc -l), 1)
	    LDFLAGS += -static-libgcc -static-libstdc++
	  endif
	endif
#######################################

# OS X
else ifeq ($(platform), osx)
	TARGET := $(TARGET_NAME)_libretro.dylib
	fpic := -fPIC
	ifeq ($(arch),ppc)
	  ENDIANNESS_DEFINES += -DMSB_FIRST
	  PLATFORM_DEFINES := -D__POWERPC__ -D__ppc__
	endif
	OSXVER = `sw_vers -productVersion | cut -d. -f 2`
	OSX_LT_MAVERICKS = `(( $(OSXVER) <= 9)) && echo "YES"`
	MINVERSION =
ifeq ($(OSX_LT_MAVERICKS),"YES")
	MINVERSION = -mmacosx-version-min=10.2
else
	MINVERSION = -mmacosx-version-min=10.7
endif
ifeq ($(shell uname -p),arm)
	MINVERSION =
endif
   ifeq ($(CROSS_COMPILE),1)
		TARGET_RULE   = -target $(LIBRETRO_APPLE_PLATFORM) -isysroot $(LIBRETRO_APPLE_ISYSROOT)
		CFLAGS   += $(TARGET_RULE)
		CPPFLAGS += $(TARGET_RULE)
		CXXFLAGS += $(TARGET_RULE)
		LDFLAGS  += $(TARGET_RULE)
		MINVERSION =
   endif
	fpic += $(MINVERSION) -stdlib=libc++
	SHARED := -dynamiclib
	TILED_RENDERING=1

# iOS
else ifneq (,$(findstring ios,$(platform)))
	TARGET := $(TARGET_NAME)_libretro_ios.dylib
	fpic := -fPIC
	SHARED := -dynamiclib
        MINVERSION :=
	ifeq ($(IOSSDK),)
	  IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
	endif
	ifeq ($(platform),ios-arm64)
	  CC = cc -arch arm64 -isysroot $(IOSSDK)
	  CXX = clang++ -arch arm64 -isysroot $(IOSSDK)
	else
	  CC = cc -arch armv7 -isysroot $(IOSSDK)
	  CXX = clang++ -arch armv7 -isysroot $(IOSSDK)
	endif
	CFLAGS += -DIOS
	ifeq ($(platform),$(filter $(platform),ios9 ios-arm64))
	  MINVERSION = -miphoneos-version-min=8.0
	else
	  MINVERSION = -miphoneos-version-min=5.0
	endif
        CFLAGS += $(MINVERSION)
	TILED_RENDERING=1

else ifeq ($(platform), tvos-arm64)
   TARGET := $(TARGET_NAME)_libretro_tvos.dylib
   fpic := -fPIC
   SHARED := -dynamiclib -DLSB_FIRST
ifeq ($(IOSSDK),)
   IOSSDK := $(shell xcodebuild -version -sdk appletvos Path)
endif
   TILED_RENDERING = 1
   CC = cc -arch arm64 -isysroot $(IOSSDK)
   CXX = c++ -arch arm64 -isysroot $(IOSSDK)

# Theos iOS
else ifeq ($(platform), theos_ios)
	DEPLOYMENT_IOSVERSION = 5.0
	TARGET = iphone:latest:$(DEPLOYMENT_IOSVERSION)
	ARCHS = armv7 armv7s
	TARGET_IPHONEOS_DEPLOYMENT_VERSION=$(DEPLOYMENT_IOSVERSION)
	THEOS_BUILD_DIR := objs
	include $(THEOS)/makefiles/common.mk
	LIBRARY_NAME = $(TARGET_NAME)_libretro_ios
	TILED_RENDERING=1

# QNX
else ifeq ($(platform), qnx)
	TARGET := $(TARGET_NAME)_libretro_$(platform).so
	fpic := -fPIC
	SHARED := -lcpp -lm -shared -Wl,-version-script=$(LIBRETRO_DIR)/link.T -Wl,-no-undefined
	CC = qcc -Vgcc_ntoarmv7le
	CXX = QCC -Vgcc_ntoarmv7le_cpp
	AR = QCC -Vgcc_ntoarmv7le
	PLATFORM_DEFINES := -D__BLACKBERRY_QNX__ -marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp
	TILED_RENDERING=1

# Lightweight PS3 Homebrew SDK
else ifeq ($(platform), psl1ght)
	TARGET := $(TARGET_NAME)_libretro_psl1ght.a
	CC = $(PS3DEV)/ppu/bin/ppu-gcc$(EXE_EXT)
	CXX = $(PS3DEV)/ppu/bin/ppu-g++$(EXE_EXT)
	AR = $(PS3DEV)/ppu/bin/ppu-ar$(EXE_EXT)
	ENDIANNESS_DEFINES += -DMSB_FIRST
	PLATFORM_DEFINES := -D__PS3__ -D__POWERPC__ -D__ppc__
	STATIC_LINKING=1
	TILED_RENDERING=1

# PSP1
else ifeq ($(platform), psp1)
	TARGET := $(TARGET_NAME)_libretro_$(platform).a
	CC = psp-gcc$(EXE_EXT)
	CXX = psp-g++$(EXE_EXT)
	AR = psp-ar$(EXE_EXT)
	PLATFORM_DEFINES := -DPSP
	CFLAGS += -G0
	CXXFLAGS += -G0
	STATIC_LINKING=1
	TILED_RENDERING=1

# Vita
else ifeq ($(platform), vita)
	TARGET := $(TARGET_NAME)_libretro_$(platform).a
	CC = arm-vita-eabi-gcc$(EXE_EXT)
	CXX = arm-vita-eabi-g++$(EXE_EXT)
	AR = arm-vita-eabi-ar$(EXE_EXT)
	PLATFORM_DEFINES := -DVITA
	__FLAGS := -marm -mfpu=neon -mtune=cortex-a9 -mcpu=cortex-a9 -mfloat-abi=hard -mword-relocations
	__FLAGS += -fno-optimize-sibling-calls -fno-strict-aliasing -fno-partial-inlining -fno-tree-vrp
	__FLAGS += -ffast-math -fsingle-precision-constant -funroll-loops -ftracer

	CFLAGS += $(__FLAGS)
	CXXFLAGS += $(__FLAGS)
	CXXFLAGS += -fno-exceptions -fno-rtti
	STATIC_LINKING=1
	TILED_RENDERING=1
	USE_CHEATS=0 #for performance boost.

	USE_THREADED_RENDERER=1
	USE_MOTION_SENSOR=1
	USE_FRAME_SKIP=1
	HAVE_NEON=1

# Libxenon
else ifeq ($(platform), xenon)
	TARGET := $(TARGET_NAME)_libretro_xenon360.a
	CC = xenon-gcc$(EXE_EXT)
	CXX = xenon-g++$(EXE_EXT)
	AR = xenon-ar$(EXE_EXT)
	ENDIANNESS_DEFINES += -DMSB_FIRST
	PLATFORM_DEFINES := -D__LIBXENON__ -D__POWERPC__ -D__ppc__
	STATIC_LINKING=1
	TILED_RENDERING=1

# Nintendo Game Cube
else ifeq ($(platform), ngc)
	TARGET := $(TARGET_NAME)_libretro_$(platform).a
	CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
	CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
	AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
	ENDIANNESS_DEFINES += -DMSB_FIRST
	PLATFORM_DEFINES += -DGEKKO -DHW_DOL -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__
	PLATFORM_DEFINES += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
	STATIC_LINKING=1
	TILED_RENDERING=1

# Nintendo Wii
else ifeq ($(platform), wii)
	TARGET := $(TARGET_NAME)_libretro_$(platform).a
	CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
	CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
	AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
	ENDIANNESS_DEFINES += -DMSB_FIRST
	PLATFORM_DEFINES += -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__
	PLATFORM_DEFINES += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
	STATIC_LINKING=1
	TILED_RENDERING=1

# Nintendo WiiU
else ifeq ($(platform), wiiu)
	TARGET := $(TARGET_NAME)_libretro_$(platform).a
	CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
	CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
	AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
	ENDIANNESS_DEFINES += -DMSB_FIRST
	PLATFORM_DEFINES += -DGEKKO -DWIIU -DHW_RVL -mwup -mcpu=750 -meabi -mhard-float -D__ppc__
	PLATFORM_DEFINES += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
	STATIC_LINKING=1
	TILED_RENDERING=1

# Nintendo Switch (libnx)
else ifeq ($(platform), libnx)
	include $(DEVKITPRO)/libnx/switch_rules
	TARGET := $(TARGET_NAME)_libretro_$(platform).a
	DEFINES := -DSWITCH=1 -U__linux__ -U__linux -DRARCH_INTERNAL -DHAVE_THREADS=1
	CFLAGS := $(DEFINES) -g -O3 \
		      -fPIE -I$(LIBNX)/include/ -ffunction-sections -fdata-sections -ftls-model=local-exec -Wl,--allow-multiple-definition -specs=$(LIBNX)/switch.specs
	CFLAGS += $(INCDIRS)
	CFLAGS += $(INCLUDE) -D__SWITCH__ -DHAVE_LIBNX
	CXXFLAGS := $(ASFLAGS) $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11
	CFLAGS += -std=gnu11
	STATIC_LINKING=1

# Nintendo Switch (libtransistor)
else ifeq ($(platform), switch)
	TARGET := $(TARGET_NAME)_libretro_$(platform).a
	include $(LIBTRANSISTOR_HOME)/libtransistor.mk
	STATIC_LINKING=1
	TILED_RENDERING=1

else ifneq (,$(findstring armv,$(platform)))
	TARGET := $(TARGET_NAME)_libretro.so
	SHARED := -shared -Wl,--no-undefined
	TILED_RENDERING=1
	fpic := -fPIC
	ifneq (,$(findstring cortexa8,$(platform)))
	  PLATFORM_DEFINES += -marm -mcpu=cortex-a8
	else ifneq (,$(findstring cortexa9,$(platform)))
	  PLATFORM_DEFINES += -marm -mcpu=cortex-a9
	endif
	PLATFORM_DEFINES += -marm
	ifneq (,$(findstring neon,$(platform)))
	  PLATFORM_DEFINES += -mfpu=neon
	  HAVE_NEON=1
	endif
	ifneq (,$(findstring softfloat,$(platform)))
	  PLATFORM_DEFINES += -mfloat-abi=softfp
	else ifneq (,$(findstring hardfloat,$(platform)))
	  PLATFORM_DEFINES += -mfloat-abi=hard
	endif
	PLATFORM_DEFINES += -DARM

# Emscripten
else ifeq ($(platform), emscripten)
	TARGET := $(TARGET_NAME)_libretro_$(platform).bc
	STATIC_LINKING=1

# Windows MSVC 2003 Xbox 1
else ifeq ($(platform), xbox1_msvc2003)
	TARGET := $(TARGET_NAME)_libretro_xdk1.lib
	CC  = CL.exe
	CXX = CL.exe
	LD  = lib.exe
	export INCLUDE := $(XDK)\xbox\include
	export LIB := $(XDK)\xbox\lib
	PATH := $(call unixcygpath,$(XDK)/xbox/bin/vc71):$(PATH)
	PSS_STYLE :=2
	CFLAGS   += -D_XBOX -D_XBOX1
	CXXFLAGS += -D_XBOX -D_XBOX1
	TILED_RENDERING=1
	STATIC_LINKING=1
	HAS_GCC := 0

# Windows MSVC 2010 Xbox 360
else ifeq ($(platform), xbox360_msvc2010)
	TARGET := $(TARGET_NAME)_libretro_xdk360.lib
	MSVCBINDIRPREFIX = $(XEDK)/bin/win32
	CC  = "$(MSVCBINDIRPREFIX)/cl.exe"
	CXX = "$(MSVCBINDIRPREFIX)/cl.exe"
	LD  = "$(MSVCBINDIRPREFIX)/lib.exe"
	export INCLUDE := $(XEDK)/include/xbox
	export LIB := $(XEDK)/lib/xbox
	PSS_STYLE :=2
	TILED_RENDERING=1
	ENDIANNESS_DEFINES += -DMSB_FIRST
	CFLAGS   += -D_XBOX -D_XBOX360
	CXXFLAGS += -D_XBOX -D_XBOX360
	STATIC_LINKING=1
	HAS_GCC := 0

# Windows MSVC 2005 x86
else ifeq ($(platform), windows_msvc2005_x86)
	CC  = cl.exe
	CXX = cl.exe
	PATH := $(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../../VC/bin"):$(PATH)
	PATH := $(PATH):$(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../IDE")
	INCLUDE := $(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../../VC/include")
	LIB := $(shell IFS=$$'\n'; cygpath -w "$(VS80COMNTOOLS)../../VC/lib")
	BIN := $(shell IFS=$$'\n'; cygpath "$(VS80COMNTOOLS)../../VC/bin")
	WindowsSdkDir := $(INETSDK)
	export INCLUDE := $(INCLUDE);$(INETSDK)/Include;libretro-common/include/compat/msvc
	export LIB := $(LIB);$(WindowsSdkDir);$(INETSDK)/Lib
	TARGET := $(TARGET_NAME)_libretro.dll
	PSS_STYLE :=2
	LDFLAGS += -DLL
	CFLAGS += -D_CRT_SECURE_NO_DEPRECATE
	CXXFLAGS += -D_CRT_SECURE_NO_DEPRECATE

# Windows MSVC 2003 x86
else ifeq ($(platform), windows_msvc2003_x86)
	CC  = cl.exe
	CXX = cl.exe
	PATH := $(shell IFS=$$'\n'; cygpath "$(VS71COMNTOOLS)../../Vc7/bin"):$(PATH)
	PATH := $(PATH):$(shell IFS=$$'\n'; cygpath "$(VS71COMNTOOLS)../IDE")
	INCLUDE := $(shell IFS=$$'\n'; cygpath "$(VS71COMNTOOLS)../../Vc7/include")
	LIB := $(shell IFS=$$'\n'; cygpath -w "$(VS71COMNTOOLS)../../Vc7/lib")
	BIN := $(shell IFS=$$'\n'; cygpath "$(VS71COMNTOOLS)../../Vc7/bin")
	WindowsSdkDir := $(INETSDK)
	export INCLUDE := $(INCLUDE);$(INETSDK)/Include;src/drivers/libretro/msvc/msvc-2005
	export LIB := $(LIB);$(WindowsSdkDir);$(INETSDK)/Lib
	TARGET := $(TARGET_NAME)_libretro.dll
	PSS_STYLE :=2
	LDFLAGS += -DLL
	CFLAGS += -D_CRT_SECURE_NO_DEPRECATE
	CXXFLAGS += -D_CRT_SECURE_NO_DEPRECATE

# Windows MSVC 2017 all architectures
else ifneq (,$(findstring windows_msvc2017,$(platform)))

	PlatformSuffix = $(subst windows_msvc2017_,,$(platform))
	ifneq (,$(findstring desktop,$(PlatformSuffix)))
		WinPartition = desktop
		MSVC2017CompileFlags = -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP -FS
		LDFLAGS += -MANIFEST -LTCG:incremental -NXCOMPAT -DYNAMICBASE -DEBUG -OPT:REF -INCREMENTAL:NO -SUBSYSTEM:WINDOWS -MANIFESTUAC:"level='asInvoker' uiAccess='false'" -OPT:ICF -ERRORREPORT:PROMPT -NOLOGO -TLBID:1
		LIBS := kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib
	else ifneq (,$(findstring uwp,$(PlatformSuffix)))
		WinPartition = uwp
		MSVC2017CompileFlags = -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WINDLL -D_UNICODE -DUNICODE -D__WRL_NO_DEFAULT_LIB__ -EHsc -FS
		LDFLAGS += -APPCONTAINER -NXCOMPAT -DYNAMICBASE -MANIFEST:NO -LTCG -OPT:REF -SUBSYSTEM:CONSOLE -MANIFESTUAC:NO -OPT:ICF -ERRORREPORT:PROMPT -NOLOGO -TLBID:1 -DEBUG:FULL -WINMD:NO
		LIBS := WindowsApp.lib
	endif

	CFLAGS += $(MSVC2017CompileFlags)
	CXXFLAGS += $(MSVC2017CompileFlags)

	TargetArchMoniker = $(subst $(WinPartition)_,,$(PlatformSuffix))

	CC  = cl.exe
	CXX = cl.exe
	LD = link.exe

	reg_query = $(call filter_out2,$(subst $2,,$(shell reg query "$2" -v "$1" 2>nul)))
	fix_path = $(subst $(SPACE),\ ,$(subst \,/,$1))

	ProgramFiles86w := $(shell cmd //c "echo %PROGRAMFILES(x86)%")
	ProgramFiles86 := $(shell cygpath "$(ProgramFiles86w)")

	WindowsSdkDir ?= $(call reg_query,InstallationFolder,HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0)
	WindowsSdkDir ?= $(call reg_query,InstallationFolder,HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0)
	WindowsSdkDir ?= $(call reg_query,InstallationFolder,HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v10.0)
	WindowsSdkDir ?= $(call reg_query,InstallationFolder,HKEY_CURRENT_USER\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v10.0)
	WindowsSdkDir := $(WindowsSdkDir)

	WindowsSDKVersion ?= $(firstword $(foreach folder,$(subst $(subst \,/,$(WindowsSdkDir)Include/),,$(wildcard $(call fix_path,$(WindowsSdkDir)Include\*))),$(if $(wildcard $(call fix_path,$(WindowsSdkDir)Include/$(folder)/um/Windows.h)),$(folder),)))$(BACKSLASH)
	WindowsSDKVersion := $(WindowsSDKVersion)

	VsInstallBuildTools = $(ProgramFiles86)/Microsoft Visual Studio/2017/BuildTools
	VsInstallEnterprise = $(ProgramFiles86)/Microsoft Visual Studio/2017/Enterprise
	VsInstallProfessional = $(ProgramFiles86)/Microsoft Visual Studio/2017/Professional
	VsInstallCommunity = $(ProgramFiles86)/Microsoft Visual Studio/2017/Community

	VsInstallRoot ?= $(shell if [ -d "$(VsInstallBuildTools)" ]; then echo "$(VsInstallBuildTools)"; fi)
	ifeq ($(VsInstallRoot), )
		VsInstallRoot = $(shell if [ -d "$(VsInstallEnterprise)" ]; then echo "$(VsInstallEnterprise)"; fi)
	endif
	ifeq ($(VsInstallRoot), )
		VsInstallRoot = $(shell if [ -d "$(VsInstallProfessional)" ]; then echo "$(VsInstallProfessional)"; fi)
	endif
	ifeq ($(VsInstallRoot), )
		VsInstallRoot = $(shell if [ -d "$(VsInstallCommunity)" ]; then echo "$(VsInstallCommunity)"; fi)
	endif
	VsInstallRoot := $(VsInstallRoot)

	VcCompilerToolsVer := $(shell cat "$(VsInstallRoot)/VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt" | grep -o '[0-9\.]*')
	VcCompilerToolsDir := $(VsInstallRoot)/VC/Tools/MSVC/$(VcCompilerToolsVer)

	WindowsSDKSharedIncludeDir := $(shell cygpath -w "$(WindowsSdkDir)\Include\$(WindowsSDKVersion)\shared")
	WindowsSDKUCRTIncludeDir := $(shell cygpath -w "$(WindowsSdkDir)\Include\$(WindowsSDKVersion)\ucrt")
	WindowsSDKUMIncludeDir := $(shell cygpath -w "$(WindowsSdkDir)\Include\$(WindowsSDKVersion)\um")
	WindowsSDKUCRTLibDir := $(shell cygpath -w "$(WindowsSdkDir)\Lib\$(WindowsSDKVersion)\ucrt\$(TargetArchMoniker)")
	WindowsSDKUMLibDir := $(shell cygpath -w "$(WindowsSdkDir)\Lib\$(WindowsSDKVersion)\um\$(TargetArchMoniker)")

	# For some reason the HostX86 compiler doesn't like compiling for x64
	# ("no such file" opening a shared library), and vice-versa.
	# Work around it for now by using the strictly x86 compiler for x86, and x64 for x64.
	# NOTE: What about ARM?
	ifneq (,$(findstring x64,$(TargetArchMoniker)))
		VCCompilerToolsBinDir := $(VcCompilerToolsDir)\bin\HostX64
	else
		VCCompilerToolsBinDir := $(VcCompilerToolsDir)\bin\HostX86
	endif

	PATH := $(shell IFS=$$'\n'; cygpath "$(VCCompilerToolsBinDir)/$(TargetArchMoniker)"):$(PATH)
	PATH := $(PATH):$(shell IFS=$$'\n'; cygpath "$(VsInstallRoot)/Common7/IDE")
	INCLUDE := $(shell IFS=$$'\n'; cygpath -w "$(VcCompilerToolsDir)/include")
	LIB := $(shell IFS=$$'\n'; cygpath -w "$(VcCompilerToolsDir)/lib/$(TargetArchMoniker)")
	ifneq (,$(findstring uwp,$(PlatformSuffix)))
		LIB := $(shell IFS=$$'\n'; cygpath -w "$(LIB)/store")
	endif

	export INCLUDE := $(INCLUDE);$(WindowsSDKSharedIncludeDir);$(WindowsSDKUCRTIncludeDir);$(WindowsSDKUMIncludeDir)
	export LIB := $(LIB);$(WindowsSDKUCRTLibDir);$(WindowsSDKUMLibDir)
	TARGET := $(TARGET_NAME)_libretro.dll
	PSS_STYLE :=2
	LDFLAGS += -DLL

# Windows
else
	TARGET := $(TARGET_NAME)_libretro.dll
	CC ?= gcc
	CXX ?= g++
	SHARED := -shared -static-libgcc -static-libstdc++ -Wl,-no-undefined -Wl,-version-script=$(LIBRETRO_DIR)/link.T
	TILED_RENDERING=1
endif

include Makefile.common

OBJS := $(SOURCES_CXX:.cpp=.o)

ifeq ($(STATIC_LINKING),1)
SHARED=
fpic=
endif

ifneq ($(SANITIZER),)
	CFLAGS += -fsanitize=$(SANITIZER)
	CXXFLAGS += -fsanitize=$(SANITIZER)
	LDFLAGS += -fsanitize=$(SANITIZER)
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g
	CXXFLAGS += -O0 -g
else
	CFLAGS += -O2 -DNDEBUG
	CXXFLAGS += -O2 -DNDEBUG
endif

CFLAGS += -Wall $(fpic) $(VBA_DEFINES) $(ENDIANNESS_DEFINES) $(PLATFORM_DEFINES)
CXXFLAGS += -Wall $(fpic) $(VBA_DEFINES) $(ENDIANNESS_DEFINES) $(PLATFORM_DEFINES)

OBJOUT   = -o
LINKOUT  = -o

ifneq (,$(findstring msvc,$(platform)))
	OBJOUT = -Fo
	LINKOUT = -out:
ifeq ($(STATIC_LINKING),1)
	LD ?= lib.exe
	STATIC_LINKING=0

	ifeq ($(DEBUG), 1)
		CFLAGS += -MTd
		CXXFLAGS += -MTd
	else
		CFLAGS += -MT
		CXXFLAGS += -MT
	endif
else
	LD = link.exe

	ifeq ($(DEBUG), 1)
		CFLAGS += -MDd
		CXXFLAGS += -MDd
	else
		CFLAGS += -MD
		CXXFLAGS += -MD
	endif
endif
else
	LD = $(CXX)
endif

%.o: %.cpp
	$(CXX) -c $(OBJOUT)$@ $< $(CXXFLAGS) $(INCFLAGS)

%.o: %.c
	$(CC) -c $(OBJOUT)$@ $< $(CFLAGS) $(INCFLAGS)

ifeq ($(platform), theos_ios)
COMMON_FLAGS := -DIOS $(COMMON_DEFINES) $(INCFLAGS) -I$(THEOS_INCLUDE_PATH) -Wno-error
$(LIBRARY_NAME)_CFLAGS += $(CFLAGS) $(COMMON_FLAGS)
$(LIBRARY_NAME)_CXXFLAGS += $(CXXFLAGS) $(COMMON_FLAGS)
${LIBRARY_NAME}_FILES = $(SOURCES_CXX) $(SOURCES_C)
include $(THEOS_MAKE_PATH)/library.mk
else
all: $(TARGET)

$(TARGET): $(OBJS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJS)
else
	$(LD) $(LINKOUT)$@ $(SHARED) $(OBJS) $(LDFLAGS) $(LIBS)
endif

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
	rm -f *.a

.PHONY: clean
endif
