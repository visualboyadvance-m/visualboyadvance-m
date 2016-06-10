#Do not use this file directly.  Always use the top level CMakeLists.txt file
# This build is much easier if we just do it here.
SET( CMAKE_CXX_FLAGS -std=gnu++11 )
# not yet implemented
option( ENABLE_CAIRO "Enable Cairo rendering for the wxWidgets port" ON )
if( WIN32 )
  # not yet implemented
  option( ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" ON )
  option( ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ON )
endif( WIN32 )
option( ENABLE_OPENAL "Enable OpenAL for the wxWidgets port" ON )

if( NOT ENABLE_CAIRO )
    ADD_DEFINITIONS (-DNO_CAIRO)
endif( NOT ENABLE_CAIRO )

if( NOT ENABLE_XAUDIO2 )
    ADD_DEFINITIONS (-DNO_XAUDIO2)
endif( NOT ENABLE_XAUDIO2 )

if(NOT ENABLE_DIRECT3D)
   ADD_DEFINITIONS(-DNO_D3D)
endif(NOT ENABLE_DIRECT3D)

if(ENABLE_OPENAL)
    FIND_PACKAGE(OpenAL REQUIRED)
    INCLUDE_DIRECTORIES(${OPENAL_INCLUDE_DIR})
else(ENABLE_OPENAL)
    ADD_DEFINITIONS (-DNO_OAL)
endif(ENABLE_OPENAL)


IF(CMAKE_BUILD_TYPE EQUAL "Debug")
    SET(wxWidgets_USE_DEBUG ON)
ENDIF()
SET(wxWidgets_USE_UNICODE ON)
# adv is for wxAboutBox
# xml, html is for xrc
SET( wxWidgets_USE_LIBS xrc xml html adv gl net core base )
#list(APPEND wxWidgets_CONFIG_OPTIONS --version=2.8)
FIND_PACKAGE ( wxWidgets REQUIRED )
#EXECUTE_PROCESS(COMMAND sh "${wxWidgets_CONFIG_EXECUTABLE}" --cxxflags)
INCLUDE( ${wxWidgets_USE_FILE} )
FIND_PACKAGE ( Gettext REQUIRED )
FIND_PROGRAM(XGETTEXT xgettext)
FIND_PROGRAM(MSGINIT msginit)
if(ENABLE_NLS AND (NOT XGETTEXT OR NOT MSGINIT))
   message(SEND_ERROR "Cannot find gettext ${XGETTEXT} ${MSGINIT}")
endif(ENABLE_NLS AND (NOT XGETTEXT OR NOT MSGINIT))
IF(ENABLE_CAIRO)
    FIND_PACKAGE ( PkgConfig REQUIRED )
    PKG_CHECK_MODULES(CAIRO REQUIRED cairo)
    include_directories(${CAIRO_INCLUDE_DIRS})
    IF(WIN32)
       # need gdiplus to extract hdc for cairo context
       SET(CAIRO_LIBRARIES ${CAIRO_LIBRARIES} -lgdiplus)
    ENDIF(WIN32)
    # SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} ${CAIRO_CFLAGS})
ELSE(ENABLE_CAIRO)
    ADD_DEFINITIONS (-DNO_CAIRO)
    SET(CAIRO_LIBRARIES )
ENDIF(ENABLE_CAIRO)
IF(WIN32 AND ENABLE_DIRECTX)
    FIND_PACKGE ( DirectX REQUIRED )
ENDIF(WIN32 AND ENABLE_DIRECTX)

# contrib widgets
include_directories(widgets)

# for out-of-tree builds, grab includes from both target and source dirs
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# wxrc does not support xrs files in -c output (> 10x compression)
# so do it manually using slow but portable bin2c.cmake script
ADD_CUSTOM_COMMAND(OUTPUT wxvbam.xrs
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMAND wxrc ${CMAKE_CURRENT_SOURCE_DIR}/xrc/*.xrc -o wxvbam.xrs
		   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/xrc/*.xrc)
ADD_CUSTOM_COMMAND(OUTPUT builtin-xrc.h
                   COMMAND ${CMAKE_COMMAND} -DINFILE=wxvbam.xrs -DOUTFILE=builtin-xrc.h -DVARNAME=builtin_xrs -P ${CMAKE_CURRENT_SOURCE_DIR}/bin2c.cmake
		   DEPENDS wxvbam.xrs)

# use a built-in vba-over.ini if no config file present
ADD_CUSTOM_COMMAND(OUTPUT builtin-over.h
                   COMMAND ${CMAKE_COMMAND} -DINFILE=${CMAKE_CURRENT_SOURCE_DIR}/../vba-over.ini -DOUTFILE=builtin-over.h -DVARNAME=builtin_over -P ${CMAKE_CURRENT_SOURCE_DIR}/bin2c.cmake
		   DEPENDS ../vba-over.ini)


# I don't like duplicating/triplicating code, so I only declare
# event handlers once, and copy them in other places they are needed
# all using portable cmake code
ADD_CUSTOM_COMMAND(OUTPUT cmdtab.cpp cmdhandlers.h cmd-evtable.h
                   COMMAND
		      ${CMAKE_COMMAND} -D OUTDIR=${CMAKE_CURRENT_BINARY_DIR} -P copy-events.cmake
		   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		   DEPENDS cmdevents.cpp)

#
# the following should be in the main file for consistency with
# other front ends, but can't due to cmake issues
# then again, the main file should be split up into separate dirs anyway
#

SET( SRC_WX
   wxvbam.cpp
   guiinit.cpp
   viewers.cpp
   gfxviewers.cpp
   cmdevents.cpp
   opts.cpp
   sys.cpp
   panel.cpp
   viewsupt.cpp
   widgets/keyedit.cpp
   widgets/joyedit.cpp
   widgets/sdljoy.cpp
   widgets/wxmisc.cpp
   # probably ought to be in common
   ../sdl/text.cpp
   # from external source with minor modifications
   widgets/checkedlistctrl.cpp
   # generated
   cmdtab.cpp
   # generated includes must be explicitly listed
   builtin-xrc.h
   builtin-over.h
   cmdhandlers.h
   cmd-evtable.h
   #Icon File
   xrc/vbam.xpm
)

SET( HDR_WX
   wxvbam.h
   drawing.h
   filters.h
   ioregs.h
   opts.h
   viewsupt.h
   wxhead.h
   widgets/wx/keyedit.h
   widgets/wx/joyedit.h
   widgets/wx/sdljoy.h
   widgets/wx/webupdatedef.h
   widgets/wx/wxmisc.h
   # probably ought to be in common
   ../sdl/text.h
   # from external source with minor modifications
   widgets/wx/checkedlistctrl.h
)

FILE(GLOB XRC_WX xrc/*.xrc)

SET( RES_WX
    ${XRC_WX}
    ../vba-over.ini
)

SET( CM_STUFF
    bin2c.cmake
    copy-events.cmake
)

IF(ENABLE_OPENAL)
    SET( SRC_WX ${SRC_WX} openal.cpp )
    SET( HDR_WX ${HDR_WX} openal.h )
ENDIF(ENABLE_OPENAL)

IF(ENABLE_XAUDIO2)
	SET( SRC_WX ${SRC_WX} xaudio2.cpp )
ENDIF(ENABLE_XAUDIO2)

IF( WIN32 )
    SET( SRC_WX ${SRC_WX} wxvbam.rc dsound.cpp )
    SET(DIRECTX_LIBRARIES -ldxguid -ldsound -lws2_32)
    # not strictly directx, but win32-related
    IF(ENABLE_DEBUGGER)
        SET(DIRECTX_LIBRARIES ${DIRECTX_LIBRARIES} -lwsock32)
    ENDIF(ENABLE_DEBUGGER)
ELSE( WIN32 )
    SET(DIRECTX_LIBRARIES )
ENDIF( WIN32 )

link_directories( ${CMAKE_BINARY_DIR} )

SET(VBAM_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icons/vbam.icns)

ADD_EXECUTABLE (
    visualboyadvance-m
    WIN32
    MACOSX_BUNDLE
    ${SRC_WX}
    ${HDR_WX}
    ${RES_WX}
    ${VBAM_ICON}
    ${CM_STUFF}
)

TARGET_LINK_LIBRARIES (
    visualboyadvance-m
    ${VBAMCORE_LIBS}
    ${wxWidgets_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${DIRECTX_LIBRARIES}
    ${CAIRO_LIBRARIES}
)
IF(WIN32)
    INSTALL(PROGRAMS ${PROJECT_BINARY_DIR}/visualboyadvance-m${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${CMAKE_BINARY_DIR})
ENDIF(WIN32)
IF(NOT WIN32 AND NOT APPLE)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/wxvbam.desktop DESTINATION share/applications)
ENDIF(NOT WIN32 AND NOT APPLE)

if(APPLE)
    # this should set ROM file types correctly
    SET_PROPERTY(TARGET visualboyadvance-m APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/icons/vbam.icns DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
    SET(MACOSX_BUNDLE_ICON_FILE vbam.icns)
    SET_SOURCE_FILES_PROPERTIES(${VBAM_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif(APPLE)

SET(WX_EXE_NAME visualboyadvance-m-wx${CMAKE_EXECUTABLE_SUFFIX})
