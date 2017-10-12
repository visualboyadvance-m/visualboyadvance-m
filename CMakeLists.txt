# The project's name is VBA-M it uses C and C++ code
PROJECT(VBA-M C CXX)

cmake_minimum_required( VERSION 2.8.12 )

IF(CMAKE_BUILD_TYPE STREQUAL "")
    SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
ENDIF()

SET(ALL_TARGETS fex visualboyadvance-m vbamcore vbam)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW) # link to full path of libs
    cmake_policy(SET CMP0005 NEW) # escapes in add_definitions
endif(COMMAND cmake_policy)

SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )
#Output all binaries at top level
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

option( ENABLE_SDL "Build the SDL port" OFF )
option( ENABLE_WX "Build the wxWidgets port" ON )
option( ENABLE_DEBUGGER "Enable the debugger" ON )
option( ENABLE_NLS "Enable translations" ON )

SET(ASM_DEFAULT OFF)

IF(NOT CMAKE_SYSTEM_PROCESSOR)
    IF(NOT CMAKE_TOOLCHAIN_FILE AND CMAKE_HOST_SYSTEM_PROCESSOR)
        SET(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
    ELSEIF(CMAKE_TOOLCHAIN_FILE MATCHES mxe)
        IF(CMAKE_TOOLCHAIN_FILE MATCHES "i[3-9]86")
            SET(CMAKE_SYSTEM_PROCESSOR i686)
        ELSE()
            SET(CMAKE_SYSTEM_PROCESSOR x86_64)
        ENDIF()
    ENDIF()
ENDIF()

# turn asm on by default on 32bit x86
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "[xX]86|i[3-9]86|[aA][mM][dD]64")
    IF(CMAKE_C_SIZEOF_DATA_PTR EQUAL 4) # 32 bit
        SET(ASM_DEFAULT ON)
        SET(X86_32 ON)
    ELSE()
        SET(AMD64 ON)
    ENDIF()
ENDIF()

OPTION(ENABLE_ASM "Enable x86 ASM related options" ${ASM_DEFAULT})

# The ARM ASM core seems to be very buggy, see #98 and #54. Default to it being
# OFF for the time being, until it is either fixed or replaced.
option( ENABLE_ASM_CORE "Enable x86 ASM CPU cores" OFF)

IF(ENABLE_ASM_CORE)
    MESSAGE(WARNING "!!!!!! The x86 ASM cores are considered buggy and dangerous, use at your own risk. !!!!!!")
ENDIF()

SET(ASM_SCALERS_DEFAULT ${ENABLE_ASM})
SET(MMX_DEFAULT ${ENABLE_ASM})

option( ENABLE_ASM_SCALERS "Enable x86 ASM graphic filters" ${ASM_SCALERS_DEFAULT})

if(ENABLE_ASM_SCALERS)
    option( ENABLE_MMX "Enable MMX" ${MMX_DEFAULT})
endif(ENABLE_ASM_SCALERS)

option( ENABLE_LINK "Enable GBA linking functionality" ON )
option( ENABLE_LIRC "Enable LIRC support" OFF )

SET(FFMPEG_DEFAULT ON)

IF(WIN32 AND NOT (MINGW AND MSYS))
    SET(FFMPEG_DEFAULT OFF)
ENDIF()

option(ENABLE_FFMPEG "Enable ffmpeg A/V recording" ${FFMPEG_DEFAULT})

SET(LTO_DEFAULT ON)

IF(WIN32 AND CMAKE_COMPILER_IS_GNUCXX AND AMD64)
    # lto produces buggy binaries for 64 bit win32
    SET(LTO_DEFAULT OFF)
ENDIF()

OPTION(ENABLE_LTO "Compile with Link Time Optimization (gcc and clang only)" ${LTO_DEFAULT})

if(MSVC)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

option( ENABLE_GBA_LOGGING "Enable extended GBA logging" ON )
if( ENABLE_GBA_LOGGING )
   ADD_DEFINITIONS (-DGBA_LOGGING )
endif( ENABLE_GBA_LOGGING )
if(ENABLE_MMX)
  ADD_DEFINITIONS (-DMMX)
endif(ENABLE_MMX)

# The SDL port can't be built without debugging support
if( NOT ENABLE_DEBUGGER AND ENABLE_SDL )
        message( SEND_ERROR "The SDL port can't be built without debugging support" )
endif( NOT ENABLE_DEBUGGER AND ENABLE_SDL )

find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    include(GitTagVersion)
    git_version(VERSION REVISION VERSION_RELEASE)

    # only use the plugin to tie the configure state to the sha to force rebuilds
    # of files that depend on version.h
    include(GetGitRevisionDescription)
    get_git_head_revision(REFSPEC COMMITHASH)
endif()

# no git or no tags, use ChangeLog
if(NOT VERSION)
    include(ChangeLogVersion)
    changelog_version(VERSION REVISION VERSION_RELEASE)
endif()

# generate version.h
include_directories(${CMAKE_BINARY_DIR})
configure_file("${CMAKE_SOURCE_DIR}/src/version.h.in" "${CMAKE_BINARY_DIR}/version.h" @ONLY)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    ADD_DEFINITIONS(-DDEBUG)
ELSE()
    ADD_DEFINITIONS(-DNDEBUG)
ENDIF()

# hack for ninja in msys2
IF(WIN32 AND CMAKE_GENERATOR STREQUAL Ninja AND (NOT $ENV{MSYSTEM} STREQUAL ""))
    SET(MSYS ON)
ENDIF()

# Add support for Homebrew, MacPorts and Fink on OS X
IF(APPLE)
    INCLUDE(MacPackageManagers)
ENDIF(APPLE)

# We do not support amd64 asm yet
IF(AMD64 AND (ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS OR ENABLE_MMX))
    MESSAGE(FATAL_ERROR "The options ASM_CORE, ASM_SCALERS and MMX are not supported on AMD64 yet.")
ENDIF()

IF(ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS)
    ENABLE_LANGUAGE(ASM_NASM)
    SET(ASM_ENABLED ON)
ENDIF()

# Look for some dependencies using CMake scripts
FIND_PACKAGE(ZLIB REQUIRED)
FIND_PACKAGE(OpenGL REQUIRED)

# TODO: make static on mac and fedora cross-compiles
FIND_PACKAGE(PNG REQUIRED)

IF(APPLE AND NOT MACPORTS)
    SET(SDL2_STATIC ON)
ENDIF()

IF(EXISTS /etc/redhat-release)
    SET(FEDORA_HOST ON)
ENDIF()

IF(WINDOWS AND FEDORA_HOST)
    SET(SDL2_STATIC ON)
ENDIF()

FIND_PACKAGE(SDL2 REQUIRED)
ADD_DEFINITIONS(${SDL2_DEFINITIONS})

if( ENABLE_LINK )
    # msys2 does not have static sfml libs atm
    # while on mxe we use static libs
    if(WIN32 AND ((NOT (MINGW AND MSYS)) OR CMAKE_TOOLCHAIN_FILE MATCHES mxe))
        set(SFML_STATIC_LIBRARIES TRUE)
    endif()
    FIND_PACKAGE ( SFML 2 COMPONENTS network system REQUIRED )
endif( ENABLE_LINK )

# set the standard libraries all ports use
SET(VBAMCORE_LIBS
    vbamcore
    fex
    ${SDL2_LIBRARY}
    ${SFML_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${PNG_LIBRARY}
)

if(ENABLE_FFMPEG)
    FIND_PACKAGE ( PkgConfig REQUIRED )

    PKG_CHECK_MODULES(FFMPEG REQUIRED libavcodec libavformat libswscale libavutil)
endif(ENABLE_FFMPEG)

if(NOT ENABLE_FFMPEG)
  ADD_DEFINITIONS(-DNO_FFMPEG)
endif(NOT ENABLE_FFMPEG)

IF( ENABLE_LIRC )
        SET( WITHLIRC 1 )
ELSE( ENABLE_LIRC )
        SET( WITHLIRC 0 )
ENDIF( ENABLE_LIRC )

INCLUDE(GNUInstallDirs)

# C defines
ADD_DEFINITIONS (-DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_ZLIB_H -DFINAL_VERSION -DSDL -DUSE_OPENGL -DSYSCONF_INSTALL_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}" -DWITH_LIRC=${WITHLIRC})
ADD_DEFINITIONS (-DPKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/vbam" -DPACKAGE=)

if( ENABLE_LINK )
    # IPC linking code needs sem_timedwait which can be either in librt or pthreads
    FIND_LIBRARY(RT_LIB rt)
    IF(RT_LIB)
       SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${RT_LIB})
       SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${RT_LIB})
    ENDIF(RT_LIB)

    FIND_LIBRARY(PTHREAD_LIB pthread)
    IF(PTHREAD_LIB)
       SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${PTHREAD_LIB})
       SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${PTHREAD_LIB})
    ENDIF(PTHREAD_LIB)

    INCLUDE(CheckFunctionExists)
    CHECK_FUNCTION_EXISTS(sem_timedwait SEM_TIMEDWAIT)
    IF( SEM_TIMEDWAIT)
        ADD_DEFINITIONS (-DHAVE_SEM_TIMEDWAIT)
    ENDIF( SEM_TIMEDWAIT)
else( ENABLE_LINK )
    ADD_DEFINITIONS (-DNO_LINK)
endif( ENABLE_LINK )

# The debugger is enabled by default
if( NOT ENABLE_DEBUGGER )
    ADD_DEFINITIONS (-DNO_DEBUGGER)
else( NOT ENABLE_DEBUGGER )
    ADD_DEFINITIONS (-DBKPT_SUPPORT)
endif( NOT ENABLE_DEBUGGER )

# The ASM core is disabled by default because we don't know on which platform we are
IF( NOT ENABLE_ASM_CORE )
    ADD_DEFINITIONS (-DC_CORE)
ENDIF( NOT ENABLE_ASM_CORE )

# Enable internationalization
if( ENABLE_NLS )
    SET( LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale )
    ADD_DEFINITIONS ( -DENABLE_NLS )
    ADD_DEFINITIONS ( -DLOCALEDIR="${LOCALEDIR}" )
    # for now, only GBALink.cpp uses gettext() directly
    IF(APPLE)
        # use Homebrew gettext if available
        IF(EXISTS "/usr/local/opt/gettext")
            SET(CMAKE_INCLUDE_PATH "${CMAKE_INCLUDE_PATH};/usr/local/opt/gettext/include")
            SET(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH};/usr/local/opt/gettext/lib")
            SET(CMAKE_PROGRAM_PATH "${CMAKE_PROGRAM_PATH};/usr/local/opt/gettext/bin")
        ENDIF()
    ENDIF()
    IF(ENABLE_LINK)
        FIND_PATH(LIBINTL_INC libintl.h)
        FIND_LIBRARY(LIBINTL_LIB intl)
        FIND_LIBRARY(LIBICONV_LIB iconv)
        IF(LIBINTL_LIB)
            SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${LIBINTL_LIB})
            SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${LIBINTL_LIB})
        ENDIF(LIBINTL_LIB)
        IF(LIBICONV_LIB)
            SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${LIBICONV_LIB})
            SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${LIBICONV_LIB})
        ENDIF(LIBICONV_LIB)
        INCLUDE(CheckFunctionExists)
        CHECK_FUNCTION_EXISTS(gettext GETTEXT_FN)
        IF(NOT (LIBINTL_INC OR GETTEXT_FN))
            MESSAGE(FATAL_ERROR "NLS requires libintl/gettext")
        ENDIF()
        INCLUDE_DIRECTORIES(${LIBINTL_INC})
    ENDIF(ENABLE_LINK)
endif( ENABLE_NLS )

# Win32 deps submodule
IF(WIN32)
    IF(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        set(git_checkout FALSE)
        find_package(Git)
        if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
            set(git_checkout TRUE)
            execute_process(COMMAND "${GIT_EXECUTABLE}" submodule update --remote --recursive RESULT_VARIABLE git_status)
        endif()

        if(NOT (git_checkout AND git_status EQUAL 0))
            message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --remote --recursive")
        endif()
    ENDIF()

    IF(MINGW)
        INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
    ELSEIF(MSVC)
        SET(DEPS_MSVC "${CMAKE_SOURCE_DIR}/dependencies/msvc")
        INCLUDE_DIRECTORIES("${DEPS_MSVC}") # for GL/glext.h and getopt.h
    ENDIF()
ENDIF()

include(ProcessorCount)
ProcessorCount(num_cpus)

# Compiler flags

IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    SET(LTO_FLAGS "")
    IF(ENABLE_LTO)
        IF(CMAKE_COMPILER_IS_GNUCXX)
            if(num_cpus GREATER 1)
                SET(LTO_FLAGS -flto=${num_cpus} -ffat-lto-objects)
            else()
                SET(LTO_FLAGS -flto -ffat-lto-objects)
            endif()
        ELSE()
            SET(LTO_FLAGS -flto)
        ENDIF()
    ENDIF(ENABLE_LTO)

    # common optimization flags
    IF(NOT (APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3))
        SET(MY_C_OPT_FLAGS -O2 -fomit-frame-pointer ${LTO_FLAGS})
    ELSE()
        # LTO and -fomit-frame-pointer generate broken binaries on Lion with XCode 4.2 tools
        SET(MY_C_OPT_FLAGS -O2)
    ENDIF()

    # only use -mtune=generic on PC architectures
    IF(X86_32 OR AMD64)
        SET(MY_C_OPT_FLAGS ${MY_C_OPT_FLAGS} -mtune=generic)
    ENDIF()

    # common debug flags
    IF(CMAKE_COMPILER_IS_GNUCXX)
        SET(MY_C_DBG_FLAGS -ggdb3 -Og)
    ELSE()
        SET(MY_C_DBG_FLAGS -g)
    ENDIF()

    # common flags
    SET(MY_C_FLAGS -pipe -Wformat -Wformat-security -D_FORTIFY_SOURCE=2 -feliminate-unused-debug-types)

    IF(NOT WIN32)
        SET(MY_C_FLAGS ${MY_C_FLAGS} -fPIC)
    ENDIF()

    INCLUDE(CheckCXXCompilerFlag)

    # check if ssp flags are supported for this version of gcc
    IF(CMAKE_COMPILER_IS_GNUCXX)
        CHECK_CXX_COMPILER_FLAG(-fstack-protector-strong  F_STACK_PROTECTOR_STRONG_FLAG)

        IF(F_STACK_PROTECTOR_STRONG_FLAG)
            SET(MY_C_FLAGS ${MY_C_FLAGS} -fstack-protector-strong)

            CHECK_CXX_COMPILER_FLAG("--param ssp-buffer-size=4" SSP_BUFFER_SIZE_FLAG)

            IF(SSP_BUFFER_SIZE_FLAG)
                # we do not add it to MY_C_FLAGS because this breaks things like CMAKE_REQUIRED_LIBRARIES
                # which misinterpret compiler flags without leading dashes
                ADD_COMPILE_OPTIONS(--param ssp-buffer-size=4)
            ENDIF()
        ENDIF()
    ENDIF()

    IF(MINGW)
        SET(MY_C_FLAGS ${MY_C_FLAGS} -static-libgcc -static-libstdc++)
    ENDIF(MINGW)

    IF(CMAKE_BUILD_TYPE STREQUAL Debug)
        SET(MY_C_FLAGS   ${MY_C_FLAGS}   ${MY_C_DBG_FLAGS} -Wall -Wextra)
    ELSE()
        SET(MY_C_FLAGS   ${MY_C_FLAGS}   ${MY_C_OPT_FLAGS} -Wno-error)
    ENDIF()

    FOREACH(C_COMPILE_FLAG ${MY_C_FLAGS})
        ADD_COMPILE_OPTIONS(${C_COMPILE_FLAG})
    ENDFOREACH()

    SET(MY_CXX_FLAGS -std=gnu++11 -fpermissive -fexceptions)

    FOREACH(ARG ${MY_CXX_FLAGS})
        SET(MY_CXX_FLAGS_STR "${MY_CXX_FLAGS_STR} ${ARG}")
    ENDFOREACH()

    # These must be set for C++ only, and we can't use generator expressions in
    # ADD_COMPILE_OPTIONS because that's a cmake 3.3 feature and we need 2.8.12
    # compat for Ubuntu 14.
    STRING(REGEX REPLACE "<FLAGS>" "<FLAGS> ${MY_CXX_FLAGS_STR} " CMAKE_CXX_COMPILE_OBJECT ${CMAKE_CXX_COMPILE_OBJECT})

    FOREACH(ARG ${MY_C_FLAGS})
        SET(MY_C_FLAGS_STR "${MY_C_FLAGS_STR} ${ARG}")
    ENDFOREACH()

    # need all flags for linking, because of -flto etc.
    SET(CMAKE_C_LINK_EXECUTABLE   "${CMAKE_C_LINK_EXECUTABLE}   ${MY_C_FLAGS_STR}")
    SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${MY_C_FLAGS_STR}")

    # for the gcc -fstack-protector* flags we need libssp
    # we also have to use the gcc- binutils for LTO to work
    IF(CMAKE_COMPILER_IS_GNUCXX)
        IF(ENABLE_LTO)
            INCLUDE(UseGCCBinUtilsWrappers)
        ENDIF(ENABLE_LTO)

        IF(WIN32)
            SET(SSP_STATIC ON)
        ENDIF(WIN32)

        FIND_PACKAGE(SSP)

        IF(SSP_LIBRARY)
            SET(MY_C_LINKER_FLAGS ${MY_C_LINKER_FLAGS} ${SSP_LIBRARY})
        ENDIF(SSP_LIBRARY)

        SET(MY_C_LINKER_FLAGS ${MY_C_LINKER_FLAGS} -Wl,-allow-multiple-definition)
    ENDIF()

    # set linker flags
    FOREACH(ARG ${MY_C_LINKER_FLAGS})
        SET(MY_C_LINKER_FLAGS_STR "${MY_C_LINKER_FLAGS_STR} ${ARG}")
    ENDFOREACH()

    SET(CMAKE_C_LINK_EXECUTABLE   "${CMAKE_C_LINK_EXECUTABLE}   ${MY_C_LINKER_FLAGS_STR}")
    SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${MY_C_LINKER_FLAGS_STR}")
ENDIF()

# Assembler flags

IF(ASM_ENABLED)
    STRING(REGEX REPLACE "<FLAGS>" "-I${CMAKE_SOURCE_DIR}/src/filters/hq/asm/ -O1 -w-orphan-labels" CMAKE_ASM_NASM_COMPILE_OBJECT ${CMAKE_ASM_NASM_COMPILE_OBJECT})
ENDIF(ASM_ENABLED)

IF(APPLE)
    ADD_DEFINITIONS(-DMACHO)
ELSEIF("${CMAKE_SYSTEM}" MATCHES "Linux")
    ADD_DEFINITIONS(-DELF)
ENDIF()

IF(AMD64)
    ADD_DEFINITIONS(-D__AMD64__)
ENDIF()

add_subdirectory (fex)

SET(SRC_MAIN
    src/Util.cpp
    src/common/ConfigManager.cpp
    src/common/dictionary.c
    src/common/iniparser.c
    src/common/Patch.cpp
    src/common/memgzio.c
    src/common/SoundSDL.cpp
)

if(MSVC)
	SET(SRC_MAIN ${SRC_MAIN} "dependencies/msvc/getopt.c")
endif(MSVC)

SET(HDR_MAIN
    src/System.h
    src/Util.h
    src/common/array.h
    src/common/ConfigManager.h
    src/common/dictionary.h
    src/common/iniparser.h
    src/common/memgzio.h
    src/common/Port.h
    src/common/SoundDriver.h
    src/common/SoundSDL.h
)

if(MSVC)
	SET(HDR_MAIN ${HDR_MAIN} "dependencies/msvc/getopt.h")
endif(MSVC)

if(ENABLE_FFMPEG)
    SET(SRC_MAIN ${SRC_MAIN} src/common/ffmpeg.cpp)
    SET(HDR_MAIN ${HDR_MAIN} src/common/ffmpeg.h)
endif(ENABLE_FFMPEG)

if(ENABLE_NLS)
    SET(HDR_MAIN ${HDR_MAIN} src/NLS.h)
endif(ENABLE_NLS)

SET(SRC_GBA
    src/gba/agbprint.cpp
    src/gba/bios.cpp
    src/gba/BreakpointStructures.cpp
    src/gba/Cheats.cpp
    src/gba/CheatSearch.cpp
    src/gba/debugger-expr-lex.cpp
    src/gba/debugger-expr-yacc.cpp
    src/gba/EEprom.cpp
    src/gba/ereader.cpp
    src/gba/Flash.cpp
    src/gba/GBA.cpp
    src/gba/GBAGfx.cpp
    src/gba/GBALink.cpp
    src/gba/GBASockClient.cpp
    src/gba/GBA-thumb.cpp
    src/gba/GBA-arm.cpp
    src/gba/gbafilter.cpp
    src/gba/Globals.cpp
    src/gba/Mode0.cpp
    src/gba/Mode1.cpp
    src/gba/Mode2.cpp
    src/gba/Mode3.cpp
    src/gba/Mode4.cpp
    src/gba/Mode5.cpp
    src/gba/RTC.cpp
    src/gba/Sound.cpp
    src/gba/Sram.cpp
)

SET(HDR_GBA
    src/gba/agbprint.h
    src/gba/bios.h
    src/gba/BreakpointStructures.h
    src/gba/Cheats.h
    src/gba/CheatSearch.h
    src/gba/debugger-expr-yacc.hpp
    src/gba/EEprom.h
    src/gba/ereader.h
    src/gba/Flash.h
    src/gba/GBA.h
    src/gba/GBAcpu.h
    src/gba/gbafilter.h
    src/gba/GBAGfx.h
    src/gba/GBAinline.h
    src/gba/GBALink.h
    src/gba/GBASockClient.h
    src/gba/Globals.h
    src/gba/RTC.h
    src/gba/Sound.h
    src/gba/Sram.h
)

SET(SRC_GB
    src/gb/GB.cpp
    src/gb/gbCheats.cpp
    src/gb/gbDis.cpp
    src/gb/gbGfx.cpp
    src/gb/gbGlobals.cpp
    src/gb/gbMemory.cpp
    src/gb/gbPrinter.cpp
    src/gb/gbSGB.cpp
    src/gb/gbSound.cpp
)

SET(HDR_GB
    src/gb/gb.h
    src/gb/gbCheats.h
    src/gb/gbCodes.h
    src/gb/gbCodesCB.h
    src/gb/gbGlobals.h
    src/gb/gbMemory.h
    src/gb/gbPrinter.h
    src/gb/gbSGB.h
    src/gb/gbSound.h
)

SET(SRC_APU
    src/apu/Blip_Buffer.cpp
    src/apu/Effects_Buffer.cpp
    src/apu/Gb_Apu.cpp
    src/apu/Gb_Apu_State.cpp
    src/apu/Gb_Oscs.cpp
    src/apu/Multi_Buffer.cpp
)

SET(HDR_APU
    src/apu/blargg_common.h #Unused(?) but in the VS projects
    src/apu/blargg_config.h #Unused(?) but in the VS projects
    src/apu/blargg_source.h
    src/apu/Blip_Buffer.h
    src/apu/Effects_Buffer.h
    src/apu/Gb_Apu.h
    src/apu/Gb_Oscs.h
    src/apu/Multi_Buffer.h
)

SET(SRC_SDL
    src/sdl/SDL.cpp
    src/sdl/filters.cpp
    src/sdl/text.cpp
    src/sdl/inputSDL.cpp
    src/sdl/expr.cpp
    src/sdl/exprNode.cpp
    src/sdl/expr-lex.cpp
)

SET(HDR_SDL
    src/sdl/filters.h
    src/sdl/text.h
    src/sdl/inputSDL.h
    src/sdl/expr.cpp.h
    src/sdl/exprNode.h
)

SET(SRC_FILTERS
    src/filters/2xSaI.cpp
    src/filters/admame.cpp
    src/filters/bilinear.cpp
    src/filters/hq2x.cpp
    src/filters/interframe.cpp
    src/filters/pixel.cpp
    src/filters/scanline.cpp
    src/filters/simpleFilter.cpp
    src/filters/xbrzfilter.cpp
    src/filters/xBRZ/xbrz.cpp
)

SET(HDR_FILTERS
    src/filters/hq2x.h
    src/filters/interp.h
    src/filters/lq2x.h
    src/filters/xBRZ/config.h
    src/filters/xBRZ/xbrz.h
)

SET(SRC_HQ_C
    src/filters/hq/c/hq_implementation.cpp
)

SET(HDR_HQ_C
    src/filters/hq/c/hq3x_pattern.h
    src/filters/hq/c/hq4x_pattern.h
    src/filters/hq/c/hq_base.h
    src/filters/hq/c/hq_shared.h
)

SET(SRC_HQ_ASM
    src/filters/hq/asm/hq3x_16.asm
    src/filters/hq/asm/hq3x_32.asm
    src/filters/hq/asm/hq4x_16.asm
    src/filters/hq/asm/hq4x_32.asm
    src/filters/hq/asm/hq3x32.cpp
)

if( ENABLE_ASM_SCALERS )
    SET(SRC_FILTERS ${SRC_FILTERS} ${SRC_HQ_ASM})
    IF(ENABLE_MMX)
        SET(SRC_FILTERS ${SRC_FILTERS} src/filters/2xSaImmx.asm)
    ENDIF(ENABLE_MMX)
else( ENABLE_ASM_SCALERS )
    SET(SRC_FILTERS ${SRC_FILTERS} ${SRC_HQ_C})
    SET(HDR_FILTERS ${HDR_FILTERS} ${HDR_HQ_C})
    ADD_DEFINITIONS ( -DNO_ASM )
endif( ENABLE_ASM_SCALERS )

if( ENABLE_DEBUGGER )
    SET(SRC_DEBUGGER
        src/gba/armdis.cpp
        src/gba/elf.cpp
        src/gba/remote.cpp
    )
    SET(HDR_DEBUGGER
        src/gba/armdis.h
        src/gba/elf.h
        src/gba/remote.h
    )
endif( ENABLE_DEBUGGER )

INCLUDE_DIRECTORIES(
    ${ZLIB_INCLUDE_DIR}
    fex
    ${PNG_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIR}
)

IF( ENABLE_FFMPEG )
    INCLUDE_DIRECTORIES(
        ${FFMPEG_INCLUDE_DIRS}
    )
ENDIF( ENABLE_FFMPEG )

ADD_LIBRARY (
    vbamcore
    STATIC
    ${PROJECT_SRCS}
    ${SRC_MAIN}
    ${HDR_MAIN}
    ${SRC_GBA}
    ${HDR_GBA}
    ${SRC_GB}
    ${HDR_GB}
    ${SRC_APU}
    ${HDR_APU}
    ${SRC_FILTERS}
    ${HDR_FILTERS}
    ${SRC_DEBUGGER}
    ${HDR_DEBUGGER}
)
set_property(TARGET vbamcore PROPERTY CXX_STANDARD 11)
set_property(TARGET vbamcore PROPERTY CXX_STANDARD_REQUIRED ON)

IF( ENABLE_SDL )
    ADD_EXECUTABLE (
        vbam
        WIN32
        ${SRC_SDL}
        ${HDR_SDL}
    )
    set_property(TARGET vbam PROPERTY CXX_STANDARD 11)
    set_property(TARGET vbam PROPERTY CXX_STANDARD_REQUIRED ON)

    IF( WIN32 )
        SET( WIN32_LIBRARIES wsock32 ws2_32 )
    ENDIF( WIN32 )

    IF( ENABLE_LIRC )
        SET( LIRC_CLIENT_LIBRARY lirc_client )
    ENDIF( ENABLE_LIRC )

    TARGET_LINK_LIBRARIES (
        vbam
        ${VBAMCORE_LIBS}
        ${WIN32_LIBRARIES}
        ${LIRC_CLIENT_LIBRARY}
    )
    INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vbam${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

    IF( WIN32 )
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
                DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}
                RENAME vbam.cfg)
    ELSE( WIN32 )
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
                DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}
                RENAME vbam.cfg)
    ENDIF( WIN32 )
ENDIF( ENABLE_SDL )

IF( ENABLE_WX )
    add_subdirectory(src/wx)
    INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/visualboyadvance-m${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
ENDIF( ENABLE_WX )

if( ENABLE_WX )
    # Native Language Support
    if( ENABLE_NLS )
        add_subdirectory(po)
    endif( ENABLE_NLS )
endif( ENABLE_WX )
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/vba-over.ini DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/vbam)

# manual pages
IF( UNIX )
    IF( ENABLE_SDL )
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/vbam.6 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man6)
    ENDIF( ENABLE_SDL )
    IF( ENABLE_WX )
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/visualboyadvance-m.6 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man6)
    ENDIF( ENABLE_WX )
ENDIF( UNIX )

# linux data and icon files
IF( NOT WIN32 AND NOT APPLE )
    IF( ENABLE_WX )
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/wx/wxvbam.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/wx/wxvbam.appdata.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/appdata)
        INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/wx/icons/sizes/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor)
    ENDIF( ENABLE_WX )
ENDIF( NOT WIN32 AND NOT APPLE )

SET(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_PACKAGE_VERSION_MAJOR "2")
SET(CPACK_PACKAGE_VERSION_MINOR "0")
SET(CPACK_PACKAGE_VERSION_PATCH "0-Git-" ${COMMITHASH})
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
INCLUDE(CPack)
