# The project's name is VBA-M it uses C and C++ code
PROJECT(VBA-M C CXX)

cmake_minimum_required( VERSION 2.6.0 )
if( COMMAND cmake_policy )
    cmake_policy( SET CMP0003 NEW )
    cmake_policy( SET CMP0005 OLD )
endif( COMMAND cmake_policy )
SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts )
#Output all binaries at top level
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

option( ENABLE_SDL "Build the SDL port" ON )
option( ENABLE_GTK "Build the GTK+ GUI" OFF )
option( ENABLE_GTK3 "Build the GTK+ GUI" OFF )
option( ENABLE_WX "Build the wxWidgets port" ON )
option( ENABLE_DEBUGGER "Enable the debugger" ON )
option( ENABLE_NLS "Enable translations" ON )
option( ENABLE_ASM_CORE "Enable x86 ASM CPU cores" OFF )
option( ENABLE_ASM_SCALERS "Enable x86 ASM graphic filters" OFF )
option( ENABLE_LINK "Enable GBA linking functionality" OFF )
option( ENABLE_LIRC "Enable LIRC support" OFF )
option( ENABLE_FFMPEG "Enable ffmpeg A/V recording" OFF )
if(ENABLE_ASM_SCALERS)
    option( ENABLE_MMX "Enable MMX" OFF )
endif(ENABLE_ASM_SCALERS)
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

# Set the version number with -DVERSION=X.X.X-uber
IF( NOT VERSION )
#Will need to change this over to git
    SET( VERSION "https://github.com/visualboyadvance-m/visualboyadvance-m" )

    #openmw does this
    if(EXISTS ${PROJECT_SOURCE_DIR}/.git)
        if(NOT EXISTS ${PROJECT_SOURCE_DIR}/.git/shallow)
            find_package(Git)
            if(GIT_FOUND)
                include(GetGitRevisionDescription)
                get_git_head_revision(REFSPEC COMMITHASH)
                SET( VERSION "GIT Commit: " ${COMMITHASH} )
            else(GIT_FOUND)
                message(WARNING "Git executable not found")
            endif(GIT_FOUND)
        else(NOT EXISTS ${PROJECT_SOURCE_DIR}/.git/shallow)
            message(STATUS "Shallow Git clone detected, not attempting to retrieve version info")
        endif(NOT EXISTS ${PROJECT_SOURCE_DIR}/.git/shallow)
    endif(EXISTS ${PROJECT_SOURCE_DIR}/.git)
ENDIF( NOT VERSION )



# Fill in SDLMAIN_LIBRARY on OS X manually to avoid using SDLMain.m
# OS X users will have to compile and install SDL from source.
if( APPLE AND ENABLE_SDL )
    ADD_DEFINITIONS (-DwxMAC_USE_CORE_GRAPHICS )
    SET(SDL2MAIN_LIBRARY "-lSDL2main")
endif( APPLE AND ENABLE_SDL )

# Add support for MacPorts and Homebrew on OS X
# and ObjectiveC code
IF(APPLE)
    SET(CMAKE_INCLUDE_PATH "${CMAKE_INCLUDE_PATH};/usr/local/include;/opt/local/include")
    SET(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH};/usr/local/lib;/opt/local/lib")
    SET(CMAKE_PROGRAM_PATH "${CMAKE_PROGRAM_PATH};/usr/local/bin;/opt/local/bin")

    link_directories("/usr/local/lib")
    include_directories("/usr/local/include")

    link_directories("/opt/local/lib")
    include_directories("/opt/local/include")

    # and compile as Objective-C++ for ObjectiveC #ifdefs
    SET(CMAKE_CXX_COMPILE_OBJECT "<CMAKE_CXX_COMPILER> -x objective-c++ <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>")
ENDIF()

# We do not support amd64 asm yet
IF((ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS OR ENABLE_MMX)
   AND CMAKE_SYSTEM_PROCESSOR MATCHES "[xX]86|[aA][mM][dD]64|[xX]64"
   AND CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
    MESSAGE(FATAL_ERROR "The options ASM_CORE, ASM_SCALERS and MMX are not supported on AMD64 yet.")
ENDIF()

# Check for nasm
if( ENABLE_ASM_SCALERS )
    IF(EXISTS "/usr/local/bin/nasm")
        SET(CMAKE_ASM_NASM_COMPILER "/usr/local/bin/nasm")
    ELSEIF(EXISTS "/opt/local/bin/nasm")
        SET(CMAKE_ASM_NASM_COMPILER "/opt/local/bin/nasm")
    ELSE()
        SET(CMAKE_ASM_NASM_COMPILER "/usr/bin/nasm")
    ENDIF()
    ENABLE_LANGUAGE(ASM_NASM)
endif( ENABLE_ASM_SCALERS )

# Look for some dependencies using CMake scripts
FIND_PACKAGE ( ZLIB REQUIRED )
FIND_PACKAGE ( PNG REQUIRED )
FIND_PACKAGE ( OpenGL REQUIRED )
FIND_PACKAGE ( SDL2 REQUIRED )

if( ENABLE_LINK )
    if( WIN32 )
        set(SFML_STATIC_LIBRARIES TRUE)
    endif( WIN32 )
    FIND_PACKAGE ( SFML 2 COMPONENTS network system )
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

# Set the default install dir
IF( NOT DATA_INSTALL_DIR )
    SET( DATA_INSTALL_DIR "share/vbam" )
ENDIF( NOT DATA_INSTALL_DIR )

SET( PKGDATADIR ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR} )

# Set the configuration file location
IF( NOT SYSCONFDIR )
    SET( SYSCONFDIR "/etc" )
ENDIF( NOT SYSCONFDIR )

# C defines
ADD_DEFINITIONS (-DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_ZLIB_H -DFINAL_VERSION -DSDL -DUSE_OPENGL -DSYSCONFDIR='"${SYSCONFDIR}"' -DWITH_LIRC='${WITHLIRC}')
ADD_DEFINITIONS (-DVERSION='"${VERSION}"' -DPKGDATADIR='"${PKGDATADIR}"' -DPACKAGE='')

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
    ADD_DEFINITIONS ( -DLOCALEDIR=\\\"${LOCALEDIR}\\\" )
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
        FIND_PATH(LIBINTL_INC libintl.h )
        FIND_LIBRARY(LIBINTL_LIB intl )
	IF(LIBINTL_LIB)
            SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${LIBINTL_LIB})
            SET(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${LIBINTL_LIB})
	ENDIF(LIBINTL_LIB)
        INCLUDE(CheckFunctionExists)
        CHECK_FUNCTION_EXISTS(gettext GETTEXT_FN)
        IF(NOT LIBINTL_INC OR NOT GETTEXT_FN)
            MESSAGE(FATAL_ERROR "NLS requires libintl/gettext")
        ENDIF(NOT LIBINTL_INC OR NOT GETTEXT_FN)
        INCLUDE_DIRECTORIES(${LIBINTL_INC})
    ENDIF(ENABLE_LINK)
endif( ENABLE_NLS )

# Compiler flags
IF(WIN32)
    SET( CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -I$(CMAKE_SOURCE_DIR)/src/filters/hq/asm/ -O1 -w-orphan-labels")
ELSEIF(APPLE)
    SET( CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -I$(CMAKE_SOURCE_DIR)/src/filters/hq/asm/ -O1 -DMACHO -w-orphan-labels")
ELSEIF("${CMAKE_SYSTEM}" MATCHES "Linux")
    SET( CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -I$(CMAKE_SOURCE_DIR)/src/filters/hq/asm/ -O1 -DELF -w-orphan-labels")
ENDIF()

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "[xX]86|[aA][mM][dD]64|[xX]64" AND CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
    ADD_DEFINITIONS( -D__AMD64__ )
    SET(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -D__AMD64__")
ENDIF()

SET( CMAKE_C_FLAGS_RELEASE "-O3")
SET( CMAKE_CXX_FLAGS_RELEASE "-O3")
SET( CMAKE_C_FLAGS_DEBUG "-g -Wall")
SET( CMAKE_CXX_FLAGS_DEBUG "-g -Wall")

if (WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static -I./dependencies/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static -I./dependencies/include")
endif (WIN32)


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

SET(HDR_MAIN
    src/AutoBuild.h
    src/System.h
    src/Util.h
    src/version.h
    src/common/array.h
    src/common/ConfigManager.h
    src/common/dictionary.h
    src/common/iniparser.h
    src/common/memgzio.h
    src/common/Port.h
    src/common/SoundDriver.h
    src/common/SoundSDL.h
    src/common/Types.h
)

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

IF( ENABLE_SDL )
    ADD_EXECUTABLE (
        vbam
        WIN32
        ${SRC_SDL}
        ${HDR_SDL}
    )

    IF( WIN32 )
        SET( WIN32_LIBRARIES wsock32 )
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
	IF( WIN32 )
		INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/visualboyadvance-m${CMAKE_EXECUTABLE_SUFFIX} DESTINATION bin)
	ELSE( WIN32 )
		INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vbam DESTINATION bin)
	ENDIF( WIN32 )
		INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
				DESTINATION ${CMAKE_INSTALL_PREFIX}/${SYSCONFDIR}
				RENAME vbam.cfg)
ENDIF( ENABLE_SDL )

IF( ENABLE_GTK OR ENABLE_GTK3 )
	add_subdirectory (src/gtk)
ENDIF( ENABLE_GTK OR ENABLE_GTK3 )

IF( ENABLE_WX )
    add_subdirectory(src/wx)
ENDIF( ENABLE_WX )

if( ENABLE_GTK OR ENABLE_GTK3 OR ENABLE_WX )
    # Native Language Support
    if( ENABLE_NLS )
        add_subdirectory(po)
    endif( ENABLE_NLS )
endif( ENABLE_GTK OR ENABLE_GTK3 OR ENABLE_WX )
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/vba-over.ini DESTINATION ${DATA_INSTALL_DIR})
IF(NOT APPLE)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/ DESTINATION share/icons/hicolor PATTERN ".svn" EXCLUDE)
ENDIF(NOT APPLE)

# manual pages
IF( UNIX )
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/vbam.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
    IF( ENABLE_GTK )
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/gvbam.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
    ENDIF( ENABLE_GTK )
    IF( ENABLE_WX )
        INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/wxvbam.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
    ENDIF( ENABLE_WX )
ENDIF( UNIX )

SET(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
SET(CPACK_PACKAGE_VERSION_MAJOR "2")
SET(CPACK_PACKAGE_VERSION_MINOR "0")
SET(CPACK_PACKAGE_VERSION_PATCH "0-Git-" ${COMMITHASH})
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
INCLUDE(CPack)
