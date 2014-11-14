# This build is much easier if we just do it here.

# not yet implemented
option( ENABLE_CAIRO "Enable Cairo rendering for the wxWidgets port" ON )
if( WIN32 )
  # not yet implemented
  option( ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" ON )
  option( ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" OFF )
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


# adv is for wxAboutBox
# xml, html is for xrc
SET( wxWidgets_USE_LIBS xrc xml html adv gl net core base )
FIND_PACKAGE ( wxWidgets REQUIRED )
EXECUTE_PROCESS(COMMAND sh "${wxWidgets_CONFIG_EXECUTABLE}" --cxxflags)
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

# external deps
SET(ICO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../win32/res/VBA.ico)
SET(ICO_PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gtk)
SET(WX_APP_ICON ${ICO_PARENT_DIR}/icons/32x32/apps/vbam.png)
#SET(WX_APP_ICON VBA_4_32x32x24.png)
SET(ICOX_FILES VBA_4_32x32x24.png)
#wxvbam.xrc now uses gvbam icon as well
#SET(ICOX_FILES ${ICOX_FILES} VBA_9_48x48x32.png)

# Extract icons using icoutils (http://www.nongnu.org/icoutils/)
# Used for main prog. icon and about dialog (in xrc file)
# or, just use the icons already extracted for gtk
ADD_CUSTOM_COMMAND(OUTPUT ${ICOX_FILES}
                   COMMAND icotool -x ${ICO_FILE}
		   DEPENDS ${ICO_FILE})

# Convert to xpm using ImageMagick (http://www.imagemagick.org)
# not executed on win32
IF( NOT WIN32 )
    FIND_PACKAGE(ImageMagick REQUIRED convert)
    ADD_CUSTOM_COMMAND(OUTPUT wxvbam.xpm
		       COMMAND ${ImageMagick_convert_EXECUTABLE} ${WX_APP_ICON} wxvbam.xpm
		       # following is done using #define in wxvbam.cpp
		       # so there is no dependency on sed
#		       COMMAND sed -i 's/wxvbam\\[/wxvbam_xpm[/;s/char \\*/const char */' wxvbam.xpm
		       DEPENDS ${WX_APP_ICON})
ENDIF( NOT WIN32 )

# wxrc does not support xrs files in -c output (> 10x compression)
# so do it manually using slow but portable bin2c.cmake script
SET(WX_XRC_ICON icons/32x32/apps/vbam.png)
ADD_CUSTOM_COMMAND(OUTPUT wxvbam.xrs
                   # doing this in its own dir prevents name prefixes
		   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/wxvbam.xrc wxvbam.xrc
		   COMMAND ${CMAKE_COMMAND} -E copy ${ICO_PARENT_DIR}/${WX_XRC_ICON} ${WX_XRC_ICON}
                   COMMAND wxrc wxvbam.xrc -o wxvbam.xrs
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	           DEPENDS ${ICO_PARENT_DIR}/${XRC_ITEM}
		   DEPENDS wxvbam.xrc)
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
)

IF(ENABLE_OPENAL)
    SET( SRC_WX ${SRC_WX} openal.cpp )
ENDIF(ENABLE_OPENAL)

IF(ENABLE_XAUDIO2)
	SET( SRC_WX ${SRC_WX} xaudio2.cpp )
ENDIF(ENABLE_XAUDIO2)

IF( WIN32 )
    SET( SRC_WX ${SRC_WX} wxvbam.rc dsound.cpp )
    SET(DIRECTX_LIBRARIES -ldxguid -ldsound)
    # not strictly directx, but win32-related
    IF(ENABLE_DEBUGGER)
        SET(DIRECTX_LIBRARIES ${DIRECTX_LIBRARIES} -lwsock32)
    ENDIF(ENABLE_DEBUGGER)
ELSE( WIN32 )
    SET(DIRECTX_LIBRARIES )
    # generated file must be explicitly listed
    SET( SRC_WX ${SRC_WX} wxvbam.xpm )
ENDIF( WIN32 )

IF(APPLE)
    # icon must be generated manually
    SET( SRC_WX ${SRC_WX} wxvbam.icns )
    # png2icns is provided with libicns (http://icns.sourceforge.net/)
    FIND_PROGRAM(PNG2ICNS png2icns)
    # note: could add more icons, if available and proper size
    SET(WX_APP_ICONS
          ${WX_APP_ICON}
          ${ICO_PARENT_DIR}/icons/16x16/apps/vbam.png)
    ADD_CUSTOM_COMMAND(OUTPUT wxvbam.icns
                       COMMAND ${PNG2ICNS} wxvbam.icns ${WX_APP_ICONS})
    SET(MACOSX_BUNDLE_ICON_FILE wxvbam.icns)
    SET_SOURCE_FILES_PROPERTIES(wxvbam.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
ENDIF(APPLE)

link_directories( ${CMAKE_BINARY_DIR} )

ADD_EXECUTABLE (
    wxvbam
    WIN32
    MACOSX_BUNDLE
    ${SRC_WX}
)

TARGET_LINK_LIBRARIES (
    wxvbam
    ${VBAMCORE_LIBS}
    ${wxWidgets_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${DIRECTX_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/wxvbam DESTINATION bin)
IF(NOT WIN32 AND NOT APPLE)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/wxvbam.desktop DESTINATION share/applications)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../gtk/icons/ DESTINATION share/icons/hicolor PATTERN ".svn" EXCLUDE)
ENDIF(NOT WIN32 AND NOT APPLE)

# for consistency with others, copy exe to top-level dir
if(WIN32)
    SET(WX_EXE_NAME wxvbam${CMAKE_EXECUTABLE_SUFFIX})
    ADD_CUSTOM_COMMAND(TARGET wxvbam POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy ${WX_EXE_NAME} ../../${WX_EXE_NAME})
else(WIN32)
if(APPLE)
    SET(WX_EXE_NAME wxvbam.app)
    # this should set ROM file types correctly
    SET_PROPERTY(TARGET wxvbam APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
else(APPLE)
    SET(WX_EXE_NAME wxvbam${CMAKE_EXECUTABLE_SUFFIX})
endif(APPLE)
    ADD_CUSTOM_COMMAND(TARGET wxvbam POST_BUILD
		       # I'd rather make this link relative, but it's too hard
                       COMMAND rm -rf ../../${WX_EXE_NAME}
                       COMMAND ln -s ${CMAKE_CURRENT_BINARY_DIR}/${WX_EXE_NAME} ../../${WX_EXE_NAME})
endif(WIN32)
