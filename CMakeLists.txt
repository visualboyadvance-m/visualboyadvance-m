cmake_minimum_required( VERSION 2.4.6 )
if( COMMAND cmake_policy )
    cmake_policy( SET CMP0003 NEW )
    cmake_policy( SET CMP0005 OLD )
endif( COMMAND cmake_policy )
SET( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts )

# Include the definition of the ASM compiler. It will look for nasm
# This is probably not needed if CMake 2.6 or above is used
INCLUDE(CMakeScripts/CMakeDetermineASMCompiler.cmake)
INCLUDE(CMakeScripts/CMakeASMInformation.cmake)

# The project's name is VBA-M it uses assembly, C and C++ code
PROJECT(VBA-M ASM C CXX)

# Both the SDL and the GTK+ frontends are built by default
# To disable building the SDL frontend add -DNO_SDL=1 on the commandline
# To disable building the GTK+ frontend add -DNO_GTK=1 on the commandline
# To disable building the debugger, add -DNO_DEBUGGER=1.
# The SDL frontend can't be built without debugger support
# To use ASM scalers, add -DUSE_ASM_SCALERS=1
# To use the ASM core, add -DUSE_ASM_CORE=1
# Set the version number with -DVERSION=X.X.X-uber

# Version number
IF( NOT VERSION )
    SET( VERSION "1.8.0-SVN" )
ENDIF( NOT VERSION )

# Fill in SDLMAIN_LIBRARY on OS X manually to avoid using SDLMain.m
# OS X users will have to compile and install SDL from source.
if( APPLE )
	SET(SDLMAIN_LIBRARY "-lSDLmain")
endif( APPLE )

# Look for some dependencies using builtin CMake scripts
FIND_PACKAGE ( ZLIB REQUIRED )
FIND_PACKAGE ( PNG REQUIRED )
FIND_PACKAGE ( OpenGL REQUIRED )
FIND_PACKAGE ( SDL REQUIRED )

# Disable looking for GTK if not going to build the GTK frontend
# so that pkg-config is not required
IF(NOT NO_GTK)
    FIND_PACKAGE ( PkgConfig REQUIRED )

    # These dependencies require pkg-config to be found
    PKG_CHECK_MODULES ( GTKMM gtkmm-2.4 )
    PKG_CHECK_MODULES ( GLIBMM glibmm-2.4 )
    PKG_CHECK_MODULES ( GIOMM giomm-2.4 )
    PKG_CHECK_MODULES ( GLADEMM libglademm-2.4 )
    PKG_CHECK_MODULES ( XV xv )
    PKG_CHECK_MODULES ( GTKGLMM gtkglextmm-x11-1.2 )
ENDIF(NOT NO_GTK)

# Check that the dependencies are met to build the SDL frontend
IF( NOT NO_SDL )
    IF( SDL_FOUND )
        SET( CAN_BUILD_VBAM 1 )
    ENDIF( SDL_FOUND )
ENDIF( NOT NO_SDL )
IF( WITH_LIRC )
        SET( WITHLIRC 1 )
ELSE( WITH_LIRC )
        SET( WITHLIRC 0 )
ENDIF( WITH_LIRC )

# Check that the dependencies are met to build the GTK frontend
IF( NOT NO_GTK )
    IF( GLIBMM_FOUND AND GTKMM_FOUND AND GLADEMM_FOUND AND SDL_FOUND AND XV_FOUND )
        SET( CAN_BUILD_GVBAM 1 )
    ENDIF( GLIBMM_FOUND AND GTKMM_FOUND AND GLADEMM_FOUND AND SDL_FOUND AND XV_FOUND )
ENDIF( NOT NO_GTK )

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

# The debugger is enabled by default
# The SDL port can't be built without debugging support
IF( NO_DEBUGGER )
    IF( CAN_BUILD_VBAM )
        MESSAGE(STATUS "The SDL port can't be built without debugging support")
        SET( CAN_BUILD_VBAM 0 )
    ENDIF( CAN_BUILD_VBAM )

    ADD_DEFINITIONS (-DNO_DEBUGGER)
ELSE( NO_DEBUGGER )
    ADD_DEFINITIONS (-DBKPT_SUPPORT)
ENDIF( NO_DEBUGGER )

# The ASM core is disabled by default because we don't know on which platform we are
IF( NOT USE_ASM_CORE )
    ADD_DEFINITIONS (-DC_CORE)
ENDIF( NOT USE_ASM_CORE )

# Compiler flags
IF ( WIN32 )
    SET( CMAKE_ASM_FLAGS "-Isrc/hq/asm/ -O1 -w-orphan-labels")
ELSE ( WIN32 )
    SET( CMAKE_ASM_FLAGS "-Isrc/hq/asm/ -O1 -DELF -w-orphan-labels")
ENDIF ( WIN32 )
SET( CMAKE_C_FLAGS "-O3 -Wall")
SET( CMAKE_CXX_FLAGS "-O3 -Wall")

# Source files definition
SET(SRC_MAIN
    src/bios.cpp
    src/Cheats.cpp
    src/CheatSearch.cpp
    src/EEprom.cpp
    src/Flash.cpp
    src/Globals.cpp
    src/Mode0.cpp
    src/Mode1.cpp
    src/Mode2.cpp
    src/Mode3.cpp
    src/Mode4.cpp
    src/Mode5.cpp
    src/RTC.cpp
    src/Sound.cpp
    src/Sram.cpp
    src/Util.cpp
    src/common/Patch.cpp
    src/common/memgzio.c
)

SET(SRC_GBA
    src/gba/agbprint.cpp
    src/gba/GBA.cpp
    src/gba/GBAGfx.cpp
    src/gba/GBA-thumb.cpp
    src/gba/GBA-arm.cpp
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

SET(SRC_APU
    src/apu/Blip_Buffer.cpp
    src/apu/Effects_Buffer.cpp
    src/apu/Gb_Apu.cpp
    src/apu/Gb_Apu_State.cpp
    src/apu/Gb_Oscs.cpp
    src/apu/Multi_Buffer.cpp
)

SET(SRC_SDL
    src/sdl/debugger.cpp
    src/sdl/SDL.cpp
    src/sdl/dummy.cpp
    src/sdl/filters.cpp
    src/sdl/text.cpp
    src/sdl/inputSDL.cpp
    src/common/SoundSDL.cpp
    src/sdl/expr.cpp
    src/sdl/exprNode.cpp
    src/sdl/expr-lex.cpp
)

SET(SRC_FEX_MINI
    src/common/fex_mini.cpp
)

SET(SRC_FILTERS
    src/filters/2xSaI.cpp
    src/filters/admame.cpp
    src/filters/bilinear.cpp
    src/filters/hq2x.cpp
    src/filters/interframe.cpp
    src/filters/pixel.cpp
    src/filters/scanline.cpp
)

SET(SRC_HQ_C
    src/filters/hq/c/hq_implementation.cpp
)

SET(SRC_HQ_ASM
    src/filters/hq/asm/hq3x_16.asm
    src/filters/hq/asm/hq3x_32.asm
    src/filters/hq/asm/hq4x_16.asm
    src/filters/hq/asm/hq4x_32.asm
    src/filters/hq/asm/hq3x32.cpp
)

SET(SRC_GTK
    src/gtk/configfile.cpp
    src/gtk/main.cpp
    src/gtk/system.cpp
    src/gtk/windowcallbacks.cpp
    src/gtk/filters.cpp
    src/gtk/joypadconfig.cpp
    src/gtk/directoriesconfig.cpp
    src/gtk/displayconfig.cpp
    src/gtk/soundconfig.cpp
    src/gtk/screenarea.cpp
    src/gtk/screenarea-cairo.cpp
    src/gtk/screenarea-xvideo.cpp
    src/gtk/screenarea-opengl.cpp
    src/gtk/tools.cpp
    src/gtk/window.cpp
    src/sdl/inputSDL.cpp
    src/common/SoundSDL.cpp
)

IF( NOT NO_DEBUGGER )
    SET(SRC_DEBUGGER
        src/armdis.cpp
        src/elf.cpp
        src/remote.cpp
    )
ENDIF( NOT NO_DEBUGGER )

IF(CMAKE_ASM_COMPILER_LOADED AND USE_ASM_SCALERS)
    SET(SRC_HQ ${SRC_HQ_ASM})
ELSE(CMAKE_ASM_COMPILER_LOADED AND USE_ASM_SCALERS)
    SET(SRC_HQ ${SRC_HQ_C})
ENDIF(CMAKE_ASM_COMPILER_LOADED AND USE_ASM_SCALERS)

INCLUDE_DIRECTORIES(
    ${ZLIB_INCLUDE_DIR}
    ${PNG_INCLUDE_DIR}
    ${SDL_INCLUDE_DIR}
)

IF( CAN_BUILD_GVBAM )
    INCLUDE_DIRECTORIES(
        ${GLIBMM_INCLUDE_DIRS}
        ${GIOMM_INCLUDE_DIRS}
        ${GTKMM_INCLUDE_DIRS}
        ${GTKGLMM_INCLUDE_DIRS}
        ${GLADEMM_INCLUDE_DIRS}
    )

    LINK_DIRECTORIES(
        ${GLIBMM_LIBRARY_DIRS}
        ${GIOMM_LIBRARY_DIRS}
        ${GTKMM_LIBRARY_DIRS}
        ${GTKGLMM_LIBRARY_DIRS}
        ${GLADEMM_LIBRARY_DIRS}
    )
ENDIF( CAN_BUILD_GVBAM )

ADD_LIBRARY (
    vbamcore
    ${PROJECT_SRCS}
    ${SRC_MAIN}
    ${SRC_GBA}
    ${SRC_GB}
    ${SRC_APU}
    ${SRC_FEX_MINI}
    ${SRC_FILTERS}
    ${SRC_DEBUGGER}
)

IF( CAN_BUILD_VBAM )
    ADD_EXECUTABLE (
        vbam
        WIN32
        ${SRC_SDL}
        ${SRC_HQ}
    )

    IF( WIN32 )
        SET( WIN32_LIBRARIES wsock32 )
    ENDIF( WIN32 )

    IF( WITH_LIRC )
        SET( LIRC_CLIENT_LIBRARY lirc_client )
    ENDIF( WITH_LIRC )

    TARGET_LINK_LIBRARIES (
        vbam
        vbamcore
        ${SDL_LIBRARY}
        ${ZLIB_LIBRARY}
        ${PNG_LIBRARY}
        ${OPENGL_LIBRARY}
        ${WIN32_LIBRARIES}
        ${LIRC_CLIENT_LIBRARY}
    )

    INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vbam DESTINATION bin)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
            DESTINATION ${SYSCONFDIR}
            RENAME vbam.cfg)

ENDIF( CAN_BUILD_VBAM )

IF( CAN_BUILD_GVBAM )
    ADD_EXECUTABLE (
        gvbam
        WIN32
        MACOSX_BUNDLE
        ${SRC_GTK}
    )

    TARGET_LINK_LIBRARIES (
        gvbam
        vbamcore
        ${ZLIB_LIBRARY}
        ${PNG_LIBRARY}
        ${GLADEMM_LIBRARIES}
        ${SDL_LIBRARY}
        ${XV_LIBRARIES}
        ${GTKGLMM_LIBRARIES}
    )

    INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/gvbam DESTINATION bin)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/gvbam.desktop DESTINATION share/applications)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/ DESTINATION share/icons/hicolor PATTERN ".svn" EXCLUDE)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/ui DESTINATION ${DATA_INSTALL_DIR} PATTERN ".svn" EXCLUDE)

ENDIF( CAN_BUILD_GVBAM )
