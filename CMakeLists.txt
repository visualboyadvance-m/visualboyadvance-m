cmake_minimum_required(VERSION 2.4.0)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts)
INCLUDE(CMakeScripts/CMakeDetermineASMCompiler.cmake)
INCLUDE(CMakeScripts/CMakeASMInformation.cmake)

PROJECT(VBA-M ASM C CXX)

FIND_PACKAGE ( ZLIB REQUIRED )
FIND_PACKAGE ( PNG REQUIRED )
FIND_PACKAGE ( OpenGL REQUIRED )
FIND_PACKAGE ( PkgConfig REQUIRED )

PKG_CHECK_MODULES ( GTKMM gtkmm-2.4 )
PKG_CHECK_MODULES ( GLIBMM glibmm-2.4 )
PKG_CHECK_MODULES ( GLADEMM libglademm-2.4 )
PKG_CHECK_MODULES ( PORTAUDIO portaudio-2.0 )
PKG_CHECK_MODULES ( SDL sdl )
PKG_CHECK_MODULES ( XV xv )
PKG_CHECK_MODULES ( GTKGLMM gtkglextmm-x11-1.2 )

IF( SDL_FOUND )
    SET( CAN_BUILD_VBAM 1 )
ENDIF( SDL_FOUND )

IF( GLIBMM_FOUND AND GTKMM_FOUND AND GLADEMM_FOUND AND PORTAUDIO_FOUND AND XV_FOUND )
    SET( CAN_BUILD_GVBAM 1 )
ENDIF( GLIBMM_FOUND AND GTKMM_FOUND AND GLADEMM_FOUND AND PORTAUDIO_FOUND AND XV_FOUND )

IF( NOT DATA_INSTALL_DIR )
    SET( DATA_INSTALL_DIR "src/gtk" )
    SET( PKGDATADIR ${DATA_INSTALL_DIR} )
ELSE( NOT DATA_INSTALL_DIR )
    SET( PKGDATADIR ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR} )
ENDIF( NOT DATA_INSTALL_DIR )

IF( NOT SYSCONFDIR )
    SET( SYSCONFDIR "/etc" )
ENDIF( NOT SYSCONFDIR )

IF( NOT VERSION )
    SET( VERSION "1.8.0-SVN" )
ENDIF( NOT VERSION )

ADD_DEFINITIONS (-DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_ZLIB_H -DFINAL_VERSION -DBKPT_SUPPORT -DSDL -DUSE_OPENGL -DC_CORE -DSYSCONFDIR='"${SYSCONFDIR}"')

ADD_DEFINITIONS (-DVERSION='"${VERSION}"' -DPKGDATADIR='"${PKGDATADIR}"' -DPACKAGE='')

SET( CMAKE_ASM_FLAGS "-Isrc/hq/asm/ -O1 -DELF")
SET( CMAKE_C_FLAGS "-O3")
SET( CMAKE_CXX_FLAGS "-O3")

SET(SRC_MAIN
    src/2xSaI.cpp
    src/admame.cpp
    src/armdis.cpp
    src/bilinear.cpp
    src/bios.cpp
    src/Cheats.cpp
    src/CheatSearch.cpp
    src/EEprom.cpp src/elf.cpp
    src/Flash.cpp
    src/Globals.cpp
    src/interframe.cpp
    src/hq2x.cpp
    src/Mode0.cpp
    src/Mode1.cpp
    src/Mode2.cpp
    src/Mode3.cpp
    src/Mode4.cpp
    src/Mode5.cpp
    src/pixel.cpp
    src/remote.cpp
    src/RTC.cpp
    src/scanline.cpp
    src/Sound.cpp
    src/Sram.cpp
    src/Util.cpp
    src/expr.cpp
    src/exprNode.cpp
    src/expr-lex.cpp
    src/memgzio.c
)

SET(SRC_AGB
    src/agb/agbprint.cpp
    src/agb/GBA.cpp
    src/agb/gbafilter.cpp
    src/agb/GBAGfx.cpp
    src/agb/GBA-thumb.cpp
    src/agb/GBA-arm.cpp
)

SET(SRC_DMG
    src/dmg/GB.cpp
    src/dmg/gbCheats.cpp
    src/dmg/gbDis.cpp
    src/dmg/gbGfx.cpp
    src/dmg/gbGlobals.cpp
    src/dmg/gbMemory.cpp
    src/dmg/gbPrinter.cpp
    src/dmg/gbSGB.cpp
    src/dmg/gbSound.cpp
    src/dmg/gb_apu/Blip_Buffer.cpp
    src/dmg/gb_apu/Effects_Buffer.cpp
    src/dmg/gb_apu/Gb_Apu.cpp
    src/dmg/gb_apu/Gb_Apu_State.cpp
    src/dmg/gb_apu/Gb_Oscs.cpp
    src/dmg/gb_apu/Multi_Buffer.cpp
)

SET(SRC_SDL
    src/sdl/debugger.cpp
    src/sdl/SDL.cpp
    src/sdl/dummy.cpp
    src/sdl/filters.cpp
    src/sdl/text.cpp
    src/sdl/sndSDL.cpp
)

SET(SRC_FEX_MINI
    src/fex_mini.cpp
)

SET(SRC_HQ_C
    src/hq/c/hq_implementation.cpp
)

SET(SRC_HQ_ASM
    src/hq/asm/hq3x_16.asm
    src/hq/asm/hq3x_32.asm
    src/hq/asm/hq4x_16.asm
    src/hq/asm/hq4x_32.asm
    src/hq/asm/hq3x32.cpp
)

SET(SRC_GTK
    src/gtk/configfile.cpp
    src/gtk/input.cpp
    src/gtk/main.cpp
    src/gtk/system.cpp
    src/gtk/windowcallbacks.cpp
    src/gtk/filters.cpp
    src/gtk/joypadconfig.cpp
    src/gtk/screenarea.cpp
    src/gtk/screenarea-gtk.cpp
    src/gtk/screenarea-xvideo.cpp
    src/gtk/screenarea-opengl.cpp
    src/gtk/tools.cpp
    src/gtk/window.cpp
    src/gtk/sndPortAudio.cpp
)

IF(CMAKE_ASM_COMPILER_LOADED AND USEASM)
    SET(SRC_HQ ${SRC_HQ_ASM})
ELSE(CMAKE_ASM_COMPILER_LOADED AND USEASM)
    SET(SRC_HQ ${SRC_HQ_C})
ENDIF(CMAKE_ASM_COMPILER_LOADED AND USEASM)

include_directories(
    ${GTKMM_INCLUDE_DIRS}
    ${GLADEMM_INCLUDE_DIRS}
    ${SDL_INCLUDE_DIRS}
    ${GTKGLMM_INCLUDE_DIRS}
)

ADD_LIBRARY (
    vbamcore
    ${PROJECT_SRCS}
    ${SRC_MAIN}
    ${SRC_AGB}
    ${SRC_DMG}
    ${SRC_FEX_MINI}
)

IF( CAN_BUILD_VBAM )
    ADD_EXECUTABLE (
        vbam
        WIN32
        MACOSX_BUNDLE
        ${SRC_SDL}
        ${SRC_HQ}
    )

    TARGET_LINK_LIBRARIES (
        vbam
        vbamcore
        ${SDL_LIBRARIES}
        ${ZLIB_LIBRARY}
        ${PNG_LIBRARY}
        ${OPENGL_LIBRARY}
    )
    
    INSTALL(PROGRAMS vbam DESTINATION bin)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/VisualBoyAdvance.cfg-example
            DESTINATION ${SYSCONFDIR}
            RENAME VisualBoyAdvance.cfg)
    
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
        ${PORTAUDIO_LIBRARIES}
        ${XV_LIBRARIES}
        ${GTKGLMM_LIBRARIES}
    )
    
    INSTALL(PROGRAMS gvbam DESTINATION bin)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/vba.glade DESTINATION ${DATA_INSTALL_DIR})
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/gtk/icons/gbadvance.svg DESTINATION ${DATA_INSTALL_DIR}/icons)

ENDIF( CAN_BUILD_GVBAM )
