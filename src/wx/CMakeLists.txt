#Do not use this file directly.  Always use the top level CMakeLists.txt file
# This build is much easier if we just do it here.

IF(NOT CMAKE_VERSION VERSION_LESS 3.0)
    cmake_policy(SET CMP0043 NEW) # for wxWidgets
ENDIF()

include(VbamFunctions)

if( WIN32 )
  # not yet implemented
  option( ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" ON )
  option( ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ON )
endif( WIN32 )

option(ENABLE_OPENAL "Enable OpenAL for the wxWidgets port" OFF)

IF(APPLE)
    ADD_DEFINITIONS(-DwxMAC_USE_CORE_GRAPHICS)
ENDIF(APPLE)

if( NOT ENABLE_XAUDIO2 )
    ADD_DEFINITIONS (-DNO_XAUDIO2)
endif( NOT ENABLE_XAUDIO2 )

if(NOT ENABLE_DIRECT3D)
   ADD_DEFINITIONS(-DNO_D3D)
endif(NOT ENABLE_DIRECT3D)

if(ENABLE_OPENAL)
    FIND_PACKAGE(OpenAL REQUIRED)
    INCLUDE_DIRECTORIES(${OPENAL_INCLUDE_DIR})

    IF(OPENAL_STATIC OR (WIN32 AND ((NOT (MINGW AND MSYS)) OR CMAKE_TOOLCHAIN_FILE MATCHES mxe)))
        ADD_DEFINITIONS(-DAL_LIBTYPE_STATIC)
    ENDIF()
else(ENABLE_OPENAL)
    ADD_DEFINITIONS (-DNO_OAL)
endif(ENABLE_OPENAL)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    SET(wxWidgets_USE_DEBUG ON) # noop if wx is compiled with --disable-debug, like in Mac Homebrew atm
ENDIF()

# on e.g. msys2 add a couple of libraries wx needs
#if(WIN32 AND (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang))
#    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -luuid -lwinspool")
#endif()

# convert msys paths like /c/foo to windows paths like c:/foo
# for variables set by FindWxWidgets
function(normalize_wx_paths)
    if(MSYS)
        unset(new_paths)
        foreach(p ${wxWidgets_LIBRARY_DIRS})
            execute_process(COMMAND cygpath -m "${p}" OUTPUT_VARIABLE p_win OUTPUT_STRIP_TRAILING_WHITESPACE)
            list(APPEND new_paths "${p_win}")
        endforeach()

        set(wxWidgets_LIBRARY_DIRS ${new_paths} PARENT_SCOPE)

        string(REGEX REPLACE "((^| )[^/]*)/([a-zA-Z])/" "\\1\\3:/" new_libs "${wxWidgets_LIBRARIES}")

        set(wxWidgets_LIBRARIES ${new_libs} PARENT_SCOPE)
    endif()
endfunction()

macro(cleanup_wx_vars)
    if(wxWidgets_CXX_FLAGS)
        list(REMOVE_ITEM wxWidgets_CXX_FLAGS -fpermissive)
    endif()
endmacro()

if(CMAKE_PREFIX_PATH)
    set(wxWidgets_CONFIG_OPTIONS "--prefix=${CMAKE_PREFIX_PATH}")
endif()

SET(wxWidgets_USE_UNICODE ON)
# adv is for wxAboutBox
# xml, html is for xrc
# do not include gl at first
set(wxWidgets_USE_LIBS xrc xml html adv net core base)
#list(APPEND wxWidgets_CONFIG_OPTIONS --version=2.8)

# Check for gtk4 then gtk3 packages first, some dists like arch rename the
# wx-config utility for these packages to e.g. wx-config-gtk3
#
# Do not do the check if the WX_CONFIG env var is set.
if(NOT WIN32 AND NOT APPLE AND "$ENV{WX_CONFIG}" STREQUAL "")
    find_program(wxWidgets_CONFIG_EXECUTABLE NAMES wx-config-gtk4 wx-config-gtk3 wx-config)
endif()

# the gl lib may not be available, and if it looks like it is we still have to
# do a compile test later
list(APPEND wxWidgets_USE_LIBS gl)
find_package(wxWidgets QUIET)
cleanup_wx_vars()
normalize_wx_paths()

SET(CHECK_WX_OPENGL FALSE)

IF(wxWidgets_FOUND)
    SET(CHECK_WX_OPENGL TRUE)
ELSE()
    SET(WX_HAS_OPENGL   FALSE)
    # the requirement check is later after the opengl compile test
ENDIF()

INCLUDE_DIRECTORIES(${wxWidgets_INCLUDE_DIRS})

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # tell wx to enable debug mode if possible, if the cmake module did not do it for us
    EXECUTE_PROCESS(COMMAND "${wxWidgets_CONFIG_EXECUTABLE} --debug=yes" RESULT_VARIABLE WX_CONFIG_DEBUG OUTPUT_QUIET ERROR_QUIET)

    IF(WX_CONFIG_DEBUG EQUAL 0)
        ADD_DEFINITIONS(-DwxDEBUG_LEVEL=1)
    ENDIF()

    # this one should be safe in non-debug builds too
    ADD_DEFINITIONS(-DWXDEBUG)
ENDIF()

FOREACH(DEF ${wxWidgets_DEFINITIONS})
    ADD_DEFINITIONS("-D${DEF}")
ENDFOREACH()

# check if this build of wx actually has OpenGL support

SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${MY_CXX_FLAGS} ${MY_C_FLAGS} ${MY_CXX_LINKER_FLAGS} ${MY_C_LINKER_FLAGS} ${wxWidgets_LIBRARIES})
SET(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} ${wxWidgets_CXX_FLAGS} ${MY_CXX_FLAGS} ${MY_C_FLAGS})

IF(WIN32)
    SET(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} "-Wl,--subsystem,console")
    SET(CMAKE_REQUIRED_LIBRARIES   ${CMAKE_REQUIRED_LIBRARIES}   "-Wl,--subsystem,console")
ENDIF()

SET(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${wxWidgets_INCLUDE_DIRS})

FOREACH(DEF ${wxWidgets_DEFINITIONS})
    SET(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} "-D${DEF}")
ENDFOREACH()

# CheckCXXSourceCompiles ignores compiler flags, so we have to stuff them into the definitions
SET(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_FLAGS} ${CMAKE_REQUIRED_DEFINITIONS})

INCLUDE(CheckCXXSourceCompiles)

IF(CHECK_WX_OPENGL)
    CHECK_CXX_SOURCE_COMPILES("
#include <wx/wxprec.h>
#include <wx/config.h>
#include <wx/glcanvas.h>

int main(int argc, char** argv) {
    wxGLCanvas canvas(NULL, wxID_ANY, NULL, wxPoint(0, 0), wxSize(300, 300), 0);
    return 0;
}" WX_HAS_OPENGL)
ENDIF()

IF(NOT WX_HAS_OPENGL)
    ADD_DEFINITIONS(-DNO_OGL)
    LIST(REMOVE_ITEM wxWidgets_USE_LIBS gl)
    find_package(wxWidgets REQUIRED)
    normalize_wx_paths()
    cleanup_wx_vars()
ENDIF()

IF(CMAKE_COMPILER_IS_GNUCXX AND NOT CMAKE_CROSSCOMPILING)
    SET(WX_ABI_FOUND_MATCH FALSE)

    INCLUDE(CheckCXXSourceRuns)

    SET(WX_TEST_CONSOLE_APP "
#include <cstdlib>
#include <iostream>
#include <wx/wxprec.h>
#include <wx/wx.h>

#ifdef _WIN32
#include <windows.h>
#include \"MinHook.h\"

typedef int (WINAPI *MESSAGEBOXW)(HWND, LPCWSTR, LPCWSTR, UINT);
typedef int (WINAPI *MESSAGEBOXA)(HWND, LPCSTR,  LPCSTR,  UINT);

// Pointers for calling original MessageBoxW/A.
MESSAGEBOXW fpMessageBoxW = NULL;
MESSAGEBOXA fpMessageBoxA = NULL;

// Detour function which overrides MessageBoxW.
int WINAPI DetourMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
    std::wcerr << lpCaption << \": \" << lpText << std::endl;
}

// Detour function which overrides MessageBoxA.
int WINAPI DetourMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    std::cerr << lpCaption << \": \" << lpText << std::endl;
}
#endif

class MyApp : public wxAppConsole {
public:
    virtual bool OnInit();
};

bool MyApp::OnInit() {
    exit(0);
}

wxIMPLEMENT_APP_NO_MAIN(MyApp);

int main(int argc, char** argv)
{
#ifdef _WIN32
    // just in case (this does nothing though)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    // we need to install a trampoline for messageboxes, because Wx is
    // hard-coded to show a messagebox on wxLogFatalError()

    // Initialize MinHook (for trampoline).
    if (MH_Initialize() != MH_OK) return 1;

    // Create a hook for MessageBoxW and MessageBoxA
    if (MH_CreateHook(&MessageBoxW, &DetourMessageBoxW, reinterpret_cast<LPVOID*>(&fpMessageBoxW)) != MH_OK)
        return 1;
    if (MH_CreateHook(&MessageBoxA, &DetourMessageBoxA, reinterpret_cast<LPVOID*>(&fpMessageBoxA)) != MH_OK)
        return 1;

    if (MH_EnableHook(&MessageBoxW) != MH_OK) return 1;
    if (MH_EnableHook(&MessageBoxA) != MH_OK) return 1;
#endif

    wxEntry(argc, argv);
    wxEntryCleanup();
    return 0;
}
")

    # on windows we need the trampoline library from dependencies
    IF(WIN32)
        # minhook requires -fpermissive unfortunately
        set(CMAKE_REQUIRED_FLAGS       ${CMAKE_REQUIRED_FLAGS}       -fpermissive)
        SET(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} -fpermissive -w "-I${CMAKE_SOURCE_DIR}/dependencies/minhook/include")
        SET(CMAKE_REQUIRED_LIBRARIES   ${CMAKE_REQUIRED_LIBRARIES}   -Wl,--subsystem,console)

        IF(AMD64)
            SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} "${CMAKE_SOURCE_DIR}/dependencies/minhook/libMinHook_64.a")
        ELSE() # assume 32 bit windows
            SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} "${CMAKE_SOURCE_DIR}/dependencies/minhook/libMinHook.a")
        ENDIF()
    ENDIF()

    CHECK_CXX_SOURCE_RUNS("${WX_TEST_CONSOLE_APP}" WX_DEFAULT_ABI_VERSION_COMPATIBLE)

    # remove -fpermissive set for minhook
    list(REMOVE_ITEM CMAKE_REQUIRED_FLAGS       -fpermissive)
    list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -fpermissive)

    IF(NOT WX_DEFAULT_ABI_VERSION_COMPATIBLE)
        # currently goes up to 11 with gcc7, but we give it some room
        SET(WX_ABI_VERSION 15)

        SET(CURRENT_DEFS ${CMAKE_REQUIRED_DEFINITIONS})
        SET(CURRENT_LIBS ${CMAKE_REQUIRED_LIBRARIES})

        WHILE(NOT WX_ABI_VERSION EQUAL -1)
            SET(CMAKE_REQUIRED_DEFINITIONS ${CURRENT_DEFS} "-fabi-version=${WX_ABI_VERSION}")
            SET(CMAKE_REQUIRED_LIBRARIES   ${CURRENT_LIBS} "-fabi-version=${WX_ABI_VERSION}")

            SET(WX_ABI_VAR "WX_ABI_VERSION_${WX_ABI_VERSION}")

            CHECK_CXX_SOURCE_RUNS("${WX_TEST_CONSOLE_APP}" ${WX_ABI_VAR})

            IF(${${WX_ABI_VAR}})
                SET(WX_ABI_FOUND_MATCH TRUE)
                BREAK()
            ENDIF()

            MATH(EXPR WX_ABI_VERSION "${WX_ABI_VERSION} - 1")
        ENDWHILE()

        SET(CMAKE_REQUIRED_DEFINITIONS ${CURRENT_DEFS})
        SET(CMAKE_REQUIRED_LIBRARIES   ${CURRENT_LIBS})
    ENDIF()

    IF(WX_ABI_FOUND_MATCH)
        # add C++ flags
        STRING(REGEX REPLACE "<FLAGS>" "<FLAGS> -fabi-version=${WX_ABI_VERSION} " CMAKE_CXX_COMPILE_OBJECT ${CMAKE_CXX_COMPILE_OBJECT})
        SET(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -fabi-version=${WX_ABI_VERSION}")
    ENDIF()
ENDIF()

# end of wx compile checks

FOREACH(CXX_COMPILE_FLAG ${wxWidgets_CXX_FLAGS})
    ADD_COMPILE_OPTIONS(${CXX_COMPILE_FLAG})
ENDFOREACH()

#EXECUTE_PROCESS(COMMAND sh "${wxWidgets_CONFIG_EXECUTABLE}" --cxxflags)
INCLUDE( ${wxWidgets_USE_FILE} )
FIND_PACKAGE ( Gettext REQUIRED )
FIND_PROGRAM(XGETTEXT xgettext)
FIND_PROGRAM(MSGINIT msginit)
if(ENABLE_NLS AND (NOT XGETTEXT OR NOT MSGINIT))
   message(SEND_ERROR "Cannot find gettext ${XGETTEXT} ${MSGINIT}")
endif(ENABLE_NLS AND (NOT XGETTEXT OR NOT MSGINIT))
IF(WIN32 AND ENABLE_DIRECTX)
    FIND_PACKGE ( DirectX REQUIRED )
ENDIF(WIN32 AND ENABLE_DIRECTX)

# we make some direct gtk/gdk calls on linux and such
# so need to link the gtk that wx was built with
IF(NOT WIN32 AND NOT APPLE)
    FIND_PACKAGE(PkgConfig REQUIRED)

    FIND_PATH(WX_CONFIG_H NAMES wx/config.h PATHS ${wxWidgets_INCLUDE_DIRS})
    IF(NOT WX_CONFIG_H)
        MESSAGE(FATAL_ERROR "Could not find wx/config.h in ${wxWidgets_INCLUDE_DIRS}")
    ENDIF()
    SET(WX_CONFIG_H "${WX_CONFIG_H}/wx/config.h")

    INCLUDE(CheckCXXSymbolExists)
    CHECK_CXX_SYMBOL_EXISTS(__WXGTK4__ ${WX_CONFIG_H} WX_USING_GTK4)
    CHECK_CXX_SYMBOL_EXISTS(__WXGTK3__ ${WX_CONFIG_H} WX_USING_GTK3)
    IF(WX_USING_GTK4)
        PKG_CHECK_MODULES(GTK4 REQUIRED gtk+-4.0)
        IF(NOT GTK4_INCLUDE_DIRS)
            MESSAGE(FATAL_ERROR "Could not find gtk4")
        ENDIF()
        INCLUDE_DIRECTORIES(${GTK4_INCLUDE_DIRS})
        LINK_DIRECTORIES(${GTK4_LIBRARY_DIRS})
        ADD_COMPILE_OPTIONS(${GTK4_CFLAGS_OTHER})
        SET(GTK_LIBRARIES ${GTK4_LIBRARIES})
    ELSEIF(WX_USING_GTK3)
        PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)
        IF(NOT GTK3_INCLUDE_DIRS)
            MESSAGE(FATAL_ERROR "Could not find gtk3")
        ENDIF()
        INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
        LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})
        ADD_COMPILE_OPTIONS(${GTK3_CFLAGS_OTHER})
        SET(GTK_LIBRARIES ${GTK3_LIBRARIES})
    ELSE()
        CHECK_CXX_SYMBOL_EXISTS(__WXGTK20__ ${WX_CONFIG_H} WX_USING_GTK2)
        IF(WX_USING_GTK2)
            # try to use pkg-config to find gtk2 first
            PKG_CHECK_MODULES(GTK2 REQUIRED gtk+-2.0)
            IF(GTK2_INCLUDE_DIRS)
                INCLUDE_DIRECTORIES(${GTK2_INCLUDE_DIRS})
                LINK_DIRECTORIES(${GTK2_LIBRARY_DIRS})
                ADD_COMPILE_OPTIONS(${GTK2_CFLAGS_OTHER})
                SET(GTK_LIBRARIES ${GTK2_LIBRARIES})
            ELSE()
                # and if that fails, use the cmake module
                FIND_PACKAGE(GTK2 REQUIRED gtk)
                IF(NOT GTK2_INCLUDE_DIRS)
                    MESSAGE(FATAL_ERROR "Could not find gtk2")
                ENDIF()
                INCLUDE_DIRECTORIES(${GTK2_INCLUDE_DIRS})
                ADD_COMPILE_OPTIONS(${GTK2_DEFINITIONS})
                SET(GTK_LIBRARIES ${GTK2_LIBRARIES})
            ENDIF()
        ELSE()
            FIND_PACKAGE(GTK REQUIRED gtk)
            IF(NOT GTK_INCLUDE_DIRS)
                MESSAGE(FATAL_ERROR "Could not find gtk")
            ENDIF()
            INCLUDE_DIRECTORIES(${GTK_INCLUDE_DIRS})
            ADD_COMPILE_OPTIONS(${GTK_DEFINITIONS})
        ENDIF()
    ENDIF()
ENDIF()

# contrib widgets
include_directories(widgets)

# for out-of-tree builds, grab includes from both target and source dirs
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

SET(XRC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/AccelConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/CheatAdd.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/CheatCreate.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/CheatEdit.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/CheatList.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/CodeSelect.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/DirectoriesConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/Disassemble.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/DisplayConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/ExportSPS.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBAROMInfo.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBColorPrefPanel.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBDisassemble.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBMapViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBOAMViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBPaletteViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBPrinter.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBROMInfo.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GBTileViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GameBoyAdvanceConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GameBoyConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/GeneralConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/IOViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/JoyPanel.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/JoypadConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/LinkConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/Logging.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/MainFrame.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/MainIcon.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/MainMenu.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/MapViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/MemSelRegion.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/MemViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/NetLink.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/OAMViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/PaletteViewer.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/SoundConfig.xrc
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/TileViewer.xrc
)

# wxrc does not support xrs files in -c output (> 10x compression)
# we do it using the bin2c.c utility

SET(BIN2C ${CMAKE_BINARY_DIR}/bin2c)

INCLUDE(HostCompile)

HOST_COMPILE(${CMAKE_CURRENT_SOURCE_DIR}/bin2c.c ${BIN2C})

if(WXRC)
    separate_arguments(WXRC UNIX_COMMAND "${WXRC}")
else()
    set(WXRC wxrc)
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL CYGWIN)
    # pass xrc sources through cygpath in case we only have native wxrc

    set(new_xrc_sources)

    foreach(xrc ${XRC_SOURCES})
        execute_process(COMMAND cygpath -m ${xrc} OUTPUT_VARIABLE win_path OUTPUT_STRIP_TRAILING_WHITESPACE)
        list(APPEND new_xrc_sources ${win_path})
    endforeach()

    set(XRC_SOURCES ${new_xrc_sources})
endif()

ADD_CUSTOM_COMMAND(OUTPUT wxvbam.xrs
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMAND ${WXRC} ${XRC_SOURCES} "--output=wxvbam.xrs"
                   DEPENDS ${XRC_SOURCES})

ADD_CUSTOM_COMMAND(OUTPUT builtin-xrc.h
                   COMMAND ${BIN2C} wxvbam.xrs builtin-xrc.h builtin_xrs
                   DEPENDS wxvbam.xrs)

# use a built-in vba-over.ini if no config file present
ADD_CUSTOM_COMMAND(OUTPUT builtin-over.h
                   COMMAND ${BIN2C} ${CMAKE_CURRENT_SOURCE_DIR}/../vba-over.ini builtin-over.h builtin_over
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
   wayland.cpp
   strutils.cpp
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

IF(APPLE)
    SET(SRC_WX ${SRC_WX} macsupport.mm)
ENDIF(APPLE)

SET( HDR_WX
   wxvbam.h
   drawing.h
   filters.h
   ioregs.h
   opts.h
   viewsupt.h
   wxhead.h
   wayland.h
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

SET( RES_WX
    ${XRC_SOURCES}
    ../vba-over.ini
)

SET( CM_STUFF
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
    SET(DIRECTX_LIBRARIES dxguid dsound ws2_32)
    IF(MSVC)
        # workaround for some symbols needed by static SDL2.lib
        SET(DIRECTX_LIBRARIES ${DIRECTX_LIBRARIES} imm32 version)
    ENDIF(MSVC)
    # not strictly directx, but win32-related
    IF(ENABLE_DEBUGGER)
        SET(DIRECTX_LIBRARIES ${DIRECTX_LIBRARIES} wsock32)
    ENDIF(ENABLE_DEBUGGER)
ENDIF( WIN32 )

link_directories( ${CMAKE_BINARY_DIR} )

SET(VBAM_ICON vbam.icns)

SET(VBAM_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/icons/${VBAM_ICON})

IF(APPLE)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    SET(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
ENDIF(APPLE)

ADD_EXECUTABLE (
    visualboyadvance-m
    WIN32
    MACOSX_BUNDLE
    ${SRC_WX}
    ${HDR_WX}
    ${RES_WX}
    ${VBAM_ICON_PATH}
    ${CM_STUFF}
)

TARGET_LINK_LIBRARIES (
    visualboyadvance-m
    ${VBAMCORE_LIBS}
    ${wxWidgets_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${DIRECTX_LIBRARIES}
    ${GTK_LIBRARIES}
    ${OPENAL_LIBRARY}
)

if(ENABLE_FFMPEG)
    join("${FFMPEG_LDFLAGS}" " " FFMPEG_LDFLAGS_STR)

    set_target_properties(
        visualboyadvance-m
        PROPERTIES LINK_FLAGS ${FFMPEG_LDFLAGS_STR}
    )
endif()

# link libgcc/libstdc++ statically on mingw
# and adjust link command when making a static binary
if(CMAKE_COMPILER_IS_GNUCXX)
    if(WIN32)
        # Build a console app in debug mode on Windows
        if(CMAKE_BUILD_TYPE STREQUAL Debug)
            set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -Wl,--subsystem,console")
        else()
            set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -Wl,--subsystem,windows")
        endif()
    endif()

    if(VBAM_STATIC)
        # some dists don't have a static libpthread
        set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread ")

        if(WIN32)
            add_custom_command(
                TARGET visualboyadvance-m PRE_LINK
                COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/msys-link-static.cmake
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            )
        else()
            add_custom_command(
                TARGET visualboyadvance-m PRE_LINK
                COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/link-static.cmake
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            )
        endif()
    endif()
endif()

IF(WIN32)
    INSTALL(PROGRAMS ${PROJECT_BINARY_DIR}/visualboyadvance-m${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${CMAKE_BINARY_DIR})
ENDIF(WIN32)
IF(NOT WIN32 AND NOT APPLE)
    INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/wxvbam.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
ENDIF(NOT WIN32 AND NOT APPLE)

if(APPLE)
    # this should set ROM file types correctly
    SET_PROPERTY(TARGET visualboyadvance-m APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
    SET(MACOSX_BUNDLE_ICON_FILE ${VBAM_ICON})
    SET_SOURCE_FILES_PROPERTIES(${VBAM_ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # bundle dylibs and relink them for releasing .app
    # also install translations into the .app
    # but only in Release mode
    IF(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        ADD_CUSTOM_COMMAND(TARGET visualboyadvance-m POST_BUILD
            COMMAND ${CMAKE_SOURCE_DIR}/tools/osx/third_party_libs_tool ./visualboyadvance-m.app
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        )

        add_custom_command(TARGET visualboyadvance-m POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/mac-translations.cmake
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    ENDIF()
endif(APPLE)

SET(WX_EXE_NAME visualboyadvance-m-wx${CMAKE_EXECUTABLE_SUFFIX})
