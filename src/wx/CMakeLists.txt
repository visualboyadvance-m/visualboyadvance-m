# Do not use this file directly.  Always use the top level CMakeLists.txt file

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW) # link to full path of libs
    cmake_policy(SET CMP0005 NEW) # escapes in add_definitions

    if(POLICY CMP0077)
        cmake_policy(SET CMP0077 NEW) # use vars for options
    endif()

    if(NOT CMAKE_VERSION VERSION_LESS 3.0)
        cmake_policy(SET CMP0043 NEW) # for wxWidgets, use generator expressions
    endif()
endif()

include(VbamFunctions)

if(WIN32)
    # not yet implemented
    option(ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" OFF)

    if(NOT CMAKE_TOOLCHAIN_FILE MATCHES vcpkg)
        option(ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ON)
    else()
        option(ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" OFF)
    endif()
endif()

option(ENABLE_FAUDIO "Enable FAudio sound output for the wxWidgets port" OFF)

find_package(OpenAL)

if(OPENAL_FOUND)
	set(ENABLE_OPENAL_DEFAULT ON)
else()
	set(ENABLE_OPENAL_DEFAULT OFF)
endif()

option(ENABLE_OPENAL "Enable OpenAL for the wxWidgets port" ${ENABLE_OPENAL_DEFAULT})

if(ENABLE_OPENAL)
	find_package(OpenAL REQUIRED)

	include_directories(${OPENAL_INCLUDE_DIR})

	if(OPENAL_STATIC OR (WIN32 AND ((NOT (MINGW AND MSYS)) OR CMAKE_TOOLCHAIN_FILE MATCHES mxe)))
		add_definitions(-DAL_LIBTYPE_STATIC)
	endif()
else()
    add_definitions(-DNO_OAL)
endif()

if(APPLE)
    add_definitions(-DwxMAC_USE_CORE_GRAPHICS)
endif()

if(NOT ENABLE_XAUDIO2)
    add_definitions(-DNO_XAUDIO2)
endif()

if(NOT ENABLE_FAUDIO)
    add_definitions(-DNO_FAUDIO)
endif()

if(NOT ENABLE_DIRECT3D)
    add_definitions(-DNO_D3D)
endif()

unset(FAUDIO_LIBS)
if(ENABLE_FAUDIO)
    find_package(FAudio REQUIRED)
    set(FAUDIO_LIBS FAudio)
endif()

# on unix we have to check for X11 before we overwrite all the compile/link
# flags with the wx tests
if(NOT WIN32 AND NOT APPLE)
    find_package(X11)

    if(X11_X11_LIB AND X11_Xscreensaver_LIB)
        include_directories(${X11_INCLUDE_DIR})
        set(EXTRA_X11_LIBS ${X11_X11_LIB} ${X11_Xscreensaver_LIB})
        add_definitions(-DHAVE_XSS)
    endif()
endif()

# on VS with vcpkg we can't use FindwxWidgets, we have to set everything up
# manually because the package is broken
if(WIN32 AND CMAKE_TOOLCHAIN_FILE MATCHES vcpkg AND (X86_32 OR AMD64))
    # set up wxwidgets stuff
    set(libtype u)
    set(suffix -rel)
    unset(path_prefix)

    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(libtype ud)
        set(suffix -dbg)
        set(path_prefix debug)

        add_definitions(-D_DEBUG)
    endif()

    add_definitions(-D_UNICODE -DUNICODE -DWXUSINGDLL -DwxUSE_GUI=1 -D__WXMSW__)
    include_directories(${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows/${path_prefix}/lib/msw${libtype})
    include_directories(${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows/include)
    set(wxWidgets_LIB_DIR ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows/${path_prefix}/lib)
    set(wxWidgets_LIBRARIES
        ${wxWidgets_LIB_DIR}/wxbase31${libtype}_net.lib
        ${wxWidgets_LIB_DIR}/wxbase31${libtype}_xml.lib
        ${wxWidgets_LIB_DIR}/wxmsw31${libtype}_core.lib
        ${wxWidgets_LIB_DIR}/wxmsw31${libtype}_gl.lib
        ${wxWidgets_LIB_DIR}/wxmsw31${libtype}_xrc.lib
        ${wxWidgets_LIB_DIR}/wxbase31${libtype}.lib
        winmm comctl32 oleacc rpcrt4 shlwapi version wsock32 opengl32
    )

    file(COPY ${_VCPKG_ROOT_DIR}/buildtrees/wxwidgets/${WINARCH}-windows${suffix}/lib/wxrc.exe DESTINATION ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows/${path_prefix}/bin)
    set(WXRC ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows/${path_prefix}/bin/wxrc.exe)

    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/zip.exe)
        # get zip binary for wxrc
        file(DOWNLOAD "https://www.willus.com/archive/zip64/infozip_binaries_win32.zip" ${CMAKE_CURRENT_BINARY_DIR}/infozip_binaries_win32.zip)
        # unzip it
        execute_process(
            COMMAND powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('${CMAKE_CURRENT_BINARY_DIR}/infozip_binaries_win32.zip', '${CMAKE_CURRENT_BINARY_DIR}'); }"
        )
    endif()

    # SDL2.dll does not get copied to build dir
    if(NOT EXISTS ${CMAKE_BINARY_DIR}/SDL2.dll)
        file(COPY ${_VCPKG_ROOT_DIR}/installed/${WINARCH}-windows/${path_prefix}/bin/SDL2.dll DESTINATION ${CMAKE_BINARY_DIR})
    endif()
else()
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(wxWidgets_USE_DEBUG ON) # noop if wx is compiled with --disable-debug, like in Mac Homebrew atm
    endif()

    # on e.g. msys2 add a couple of libraries wx needs
    #if(WIN32 AND (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang))
    #    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -luuid -lwinspool")
    #endif()

    if(CMAKE_PREFIX_PATH)
        set(wxWidgets_CONFIG_OPTIONS "--prefix=${CMAKE_PREFIX_PATH}")
    endif()

    set(wxWidgets_USE_UNICODE ON)

    # Check for gtk4 then gtk3 packages first, some dists like arch rename the
    # wx-config utility for these packages to e.g. wx-config-gtk3
    #
    # Do not do the check if the WX_CONFIG env var is set or the cmake variable
    # is set
    if(NOT wxWidgets_CONFIG_EXECUTABLE)
        if(DEFINED ENV{WX_CONFIG})
            separate_arguments(wxWidgets_CONFIG_EXECUTABLE UNIX_COMMAND $ENV{WX_CONFIG})
        else()
            find_wx_util(wxWidgets_CONFIG_EXECUTABLE wx-config)
        endif()
    endif()

    # adv is for wxAboutBox
    # xml, html is for xrc
    # the gl lib may not be available, and if it looks like it is we still have to
    # do a compile test later
    find_package(wxWidgets COMPONENTS xrc xml html adv net core base gl)

    if(NOT wxWidgets_FOUND)
        set(WX_HAS_OPENGL FALSE)
        find_package(wxWidgets COMPONENTS xrc xml html adv net core base REQUIRED)
    endif()

    cleanup_wx_vars()
    normalize_wx_paths()

    include_directories(${wxWidgets_INCLUDE_DIRS})

    if(CMAKE_BUILD_TYPE MATCHES Debug)
        # tell wx to enable debug mode if possible, if the cmake module did not do it for us
        execute_process(COMMAND "${wxWidgets_CONFIG_EXECUTABLE} --debug=yes" RESULT_VARIABLE WX_CONFIG_DEBUG OUTPUT_QUIET ERROR_QUIET)

        if(WX_CONFIG_DEBUG EQUAL 0)
            add_definitions(-DwxDEBUG_LEVEL=1)
        endif()

        # this one should be safe in non-debug builds too
        add_definitions(-DWXDEBUG)
    endif()

    foreach(DEF ${wxWidgets_DEFINITIONS})
        add_definitions("-D${DEF}")
    endforeach()

    foreach(CXX_COMPILE_FLAG ${wxWidgets_CXX_FLAGS})
        add_compile_options(${CXX_COMPILE_FLAG})
    endforeach()

    # set up variables for some compile/run checks for wxWidgets
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${MY_CXX_FLAGS} ${MY_C_FLAGS} ${MY_CXX_LINKER_FLAGS} ${MY_C_LINKER_FLAGS} ${wxWidgets_LIBRARIES})
    set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} ${wxWidgets_CXX_FLAGS} ${MY_CXX_FLAGS} ${MY_C_FLAGS})

    if(WIN32)
        set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} "-Wl,--subsystem,console")
        set(CMAKE_REQUIRED_LIBRARIES   ${CMAKE_REQUIRED_LIBRARIES}   "-Wl,--subsystem,console")
    endif()

    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${wxWidgets_INCLUDE_DIRS})

    foreach(DEF ${wxWidgets_DEFINITIONS})
        set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} "-D${DEF}")
    endforeach()

    # CheckCXXSourceCompiles ignores compiler flags, so we have to stuff them into the definitions
    set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_FLAGS} ${CMAKE_REQUIRED_DEFINITIONS})

    # find the right C++ ABI version for wxWidgets, this is also necessary for the OpenGL check following

    # on FreeBSD the ABI check segfaults and always fails, and we don't
    # need it because everything is built with clang
    set(ABI_CHECK FALSE)

    if((CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        AND NOT CMAKE_CROSSCOMPILING
        AND NOT CMAKE_SYSTEM_NAME STREQUAL FreeBSD)

        set(ABI_CHECK TRUE)
    endif()

    if(ABI_CHECK)
        set(WX_ABI_FOUND_MATCH FALSE)

        include(CheckCXXSourceRuns)

        set(WX_TEST_CONSOLE_APP "
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
    // this is necessary for 2.8 to make the class non-abstract
    virtual int OnRun() { return 0; }
};

bool MyApp::OnInit() {
    exit(0);
}

#if wxCHECK_VERSION(2, 9, 0)
wxIMPLEMENT_APP_NO_MAIN(MyApp);
#else
IMPLEMENT_APP_NO_MAIN(MyApp);
#endif

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
        if(WIN32)
            # minhook requires -fpermissive unfortunately
            set(CMAKE_REQUIRED_FLAGS       ${CMAKE_REQUIRED_FLAGS}       -fpermissive)
            set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} -fpermissive -w "-I${CMAKE_SOURCE_DIR}/dependencies/minhook/include")
            set(CMAKE_REQUIRED_LIBRARIES   ${CMAKE_REQUIRED_LIBRARIES}   -Wl,--subsystem,console)

            if(AMD64)
                set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} "${CMAKE_SOURCE_DIR}/dependencies/minhook/libMinHook_64.a")
            else() # assume 32 bit windows
                set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} "${CMAKE_SOURCE_DIR}/dependencies/minhook/libMinHook.a")
            endif()
        endif()

        check_cxx_source_runs("${WX_TEST_CONSOLE_APP}" WX_DEFAULT_ABI_VERSION_COMPATIBLE)

        # remove -fpermissive set for minhook
        list(REMOVE_ITEM CMAKE_REQUIRED_FLAGS       -fpermissive)
        list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -fpermissive)

        if(NOT WX_DEFAULT_ABI_VERSION_COMPATIBLE)
            # currently goes up to 11 with gcc7, but we give it some room
            set(WX_ABI_VERSION 15)

            set(CURRENT_DEFS ${CMAKE_REQUIRED_DEFINITIONS})
            set(CURRENT_LIBS ${CMAKE_REQUIRED_LIBRARIES})

            while(NOT WX_ABI_VERSION EQUAL -1)
                if(CMAKE_COMPILER_IS_GNUCXX)
                    set(CMAKE_REQUIRED_DEFINITIONS ${CURRENT_DEFS} "-fabi-version=${WX_ABI_VERSION}")
                    set(CMAKE_REQUIRED_LIBRARIES   ${CURRENT_LIBS} "-fabi-version=${WX_ABI_VERSION}")
                elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
                    set(CMAKE_REQUIRED_DEFINITIONS ${CURRENT_DEFS} "-D__GXX_ABI_VERSION=10${WX_ABI_VERSION}")
                    set(CMAKE_REQUIRED_LIBRARIES   ${CURRENT_LIBS} "-D__GXX_ABI_VERSION=10${WX_ABI_VERSION}")
                endif()

                set(WX_ABI_VAR "WX_ABI_VERSION_${WX_ABI_VERSION}")

                check_cxx_source_runs("${WX_TEST_CONSOLE_APP}" ${WX_ABI_VAR})

                if(${${WX_ABI_VAR}})
                    set(WX_ABI_FOUND_MATCH TRUE)
                    break()
                endif()

                math(EXPR WX_ABI_VERSION "${WX_ABI_VERSION} - 1")
            endwhile()

            set(CMAKE_REQUIRED_DEFINITIONS ${CURRENT_DEFS})
            set(CMAKE_REQUIRED_LIBRARIES   ${CURRENT_LIBS})
        endif()

        if(WX_ABI_FOUND_MATCH)
            # add C++ flags
            if(CMAKE_COMPILER_IS_GNUCXX)
                string(REGEX REPLACE "<FLAGS>" "<FLAGS> -fabi-version=${WX_ABI_VERSION} " CMAKE_CXX_COMPILE_OBJECT ${CMAKE_CXX_COMPILE_OBJECT})
                set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -fabi-version=${WX_ABI_VERSION}")
            elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
                string(REGEX REPLACE "<FLAGS>" "<FLAGS> -D__GXX_ABI_VERSION=10${WX_ABI_VERSION} " CMAKE_CXX_COMPILE_OBJECT ${CMAKE_CXX_COMPILE_OBJECT})
                set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -D__GXX_ABI_VERSION=10${WX_ABI_VERSION}")
            endif()
        endif()
    endif()

    # now check for OpenGL
    include(CheckCXXSourceCompiles)

    if(NOT DEFINED WX_HAS_OPENGL)
        check_cxx_source_compiles("
#include <wx/wxprec.h>
#include <wx/config.h>
#include <wx/glcanvas.h>

int main(int argc, char** argv) {
    wxGLCanvas canvas(NULL, wxID_ANY, NULL, wxPoint(0, 0), wxSize(300, 300), 0);
    return 0;
}" WX_HAS_OPENGL)
    endif()

    if(NOT WX_HAS_OPENGL)
        add_definitions(-DNO_OGL)
    endif()

    # end of wx compile checks

    # we make some direct gtk/gdk calls on linux and such
    # so need to link the gtk that wx was built with
    if(NOT WIN32 AND NOT APPLE)
        find_package(PkgConfig REQUIRED)

        find_path(WX_CONFIG_H NAMES wx/config.h PATHS ${wxWidgets_INCLUDE_DIRS})
        if(NOT WX_CONFIG_H)
            message(FATAL_ERROR "Could not find wx/config.h in ${wxWidgets_INCLUDE_DIRS}")
        endif()
        set(WX_CONFIG_H "${WX_CONFIG_H}/wx/config.h")

        include(CheckCXXSymbolExists)
        check_cxx_symbol_exists(__WXGTK4__ ${WX_CONFIG_H} WX_USING_GTK4)
        check_cxx_symbol_exists(__WXGTK3__ ${WX_CONFIG_H} WX_USING_GTK3)
        if(WX_USING_GTK4)
            pkg_check_modules(GTK4 REQUIRED gtk+-4.0)
            if(NOT GTK4_INCLUDE_DIRS)
                message(FATAL_ERROR "Could not find gtk4")
            endif()
            include_directories(${GTK4_INCLUDE_DIRS})
            link_directories(${GTK4_LIBRARY_DIRS})
            add_compile_options(${GTK4_CFLAGS_OTHER})
            set(GTK_LIBRARIES ${GTK4_LIBRARIES})
        elseif(WX_USING_GTK3)
            pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
            if(NOT GTK3_INCLUDE_DIRS)
                message(FATAL_ERROR "Could not find gtk3")
            endif()
            include_directories(${GTK3_INCLUDE_DIRS})
            link_directories(${GTK3_LIBRARY_DIRS})
            add_compile_options(${GTK3_CFLAGS_OTHER})
            set(GTK_LIBRARIES ${GTK3_LIBRARIES})
        else()
            check_cxx_symbol_exists(__WXGTK20__ ${WX_CONFIG_H} WX_USING_GTK2)
            if(WX_USING_GTK2)
                # try to use pkg-config to find gtk2 first
                pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
                if(GTK2_INCLUDE_DIRS)
                    include_directories(${GTK2_INCLUDE_DIRS})
                    link_directories(${GTK2_LIBRARY_DIRS})
                    add_compile_options(${GTK2_CFLAGS_OTHER})
                    set(GTK_LIBRARIES ${GTK2_LIBRARIES})
                else()
                    # and if that fails, use the cmake module
                    find_package(GTK2 REQUIRED gtk)
                    if(NOT GTK2_INCLUDE_DIRS)
                        message(FATAL_ERROR "Could not find gtk2")
                    endif()
                    include_directories(${GTK2_INCLUDE_DIRS})
                    add_compile_options(${GTK2_DEFINITIONS})
                    set(GTK_LIBRARIES ${GTK2_LIBRARIES})
                endif()
            else()
                find_package(GTK REQUIRED gtk)
                if(NOT GTK_INCLUDE_DIRS)
                    message(FATAL_ERROR "Could not find gtk")
                endif()
                include_directories(${GTK_INCLUDE_DIRS})
                add_compile_options(${GTK_DEFINITIONS})
            endif()
        endif()
    endif()

    if(wxWidgets_USE_FILE)
        include(${wxWidgets_USE_FILE})
    endif()
endif() # wxWidgets checks

if(CMAKE_TOOLCHAIN_FILE MATCHES vcpkg)
    find_package(unofficial-gettext REQUIRED)
else()
    find_package(Gettext REQUIRED)

    find_program(XGETTEXT xgettext)
    find_program(MSGINIT msginit)

    if(ENABLE_NLS AND (NOT XGETTEXT OR NOT MSGINIT))
       message(SEND_ERROR "Cannot find gettext ${XGETTEXT} ${MSGINIT}")
    endif()
endif()

# contrib widgets
include_directories(widgets)

# for out-of-tree builds, grab includes from both target and source dirs
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(XRC_SOURCES
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
    ${CMAKE_CURRENT_SOURCE_DIR}/xrc/SpeedupConfig.xrc
    )

# wxrc does not support xrs files in -c output (> 10x compression)
# we do it using the bin2c.c utility

if(CMAKE_CROSSCOMPILING)
    set(BIN2C ${CMAKE_BINARY_DIR}/bin2c)
else()
    set(BIN2C bin2c)
endif()

include(HostCompile)
host_compile(${CMAKE_CURRENT_SOURCE_DIR}/bin2c.c ${BIN2C})

if(WXRC)
    separate_arguments(WXRC UNIX_COMMAND ${WXRC})
elseif(DEFINED ENV{WXRC})
    separate_arguments(WXRC UNIX_COMMAND $ENV{WXRC})
elseif(wxWidgets_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${wxWidgets_CONFIG_EXECUTABLE} --utility=wxrc
        OUTPUT_VARIABLE wxrc
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # this is necessary on msys2
    if(NOT wxrc)
        execute_process(
            COMMAND sh -c "${wxWidgets_CONFIG_EXECUTABLE} --utility=wxrc"
            OUTPUT_VARIABLE wxrc
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    # check if the path from wx-config is good
    # and not e.g. an incompatible binary when cross-compiling
    if(EXISTS ${wxrc})
        check_clean_exit(exit_status ${wxrc} --help)

        if(exit_status EQUAL 0)
            set(WXRC ${wxrc})
        endif()
    elseif(wxrc)
        # this is necessary on msys2
        cygpath(cyg_path ${wxrc})

        if(EXISTS ${cyg_path})
            check_clean_exit(exit_status ${cyg_path} --help)

            if(exit_status EQUAL 0)
                set(WXRC ${cyg_path})
            endif()
        endif()
    endif()
endif()

if(NOT WXRC)
    find_wx_util(WXRC wxrc)
endif()

if(NOT WXRC)
    message(WARNING "could not find your wxrc executable")
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

add_custom_command(
    OUTPUT wxvbam.xrs
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${WXRC} ${XRC_SOURCES} "--output=wxvbam.xrs"
    DEPENDS ${XRC_SOURCES}
)

add_custom_command(
    OUTPUT builtin-xrc.h
    COMMAND ${BIN2C} wxvbam.xrs builtin-xrc.h builtin_xrs
    DEPENDS wxvbam.xrs ${BIN2C}
)

# use a built-in vba-over.ini if no config file present
add_custom_command(
    OUTPUT builtin-over.h
    COMMAND ${BIN2C} ${CMAKE_CURRENT_SOURCE_DIR}/../vba-over.ini builtin-over.h builtin_over
    DEPENDS ../vba-over.ini ${BIN2C}
)

# I don't like duplicating/triplicating code, so I only declare
# event handlers once, and copy them in other places they are needed
# all using portable cmake code
add_custom_command(
    OUTPUT cmdtab.cpp cmdhandlers.h cmd-evtable.h
    COMMAND ${CMAKE_COMMAND} -D OUTDIR=${CMAKE_CURRENT_BINARY_DIR} -P copy-events.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS cmdevents.cpp
)


# the following should be in the main file for consistency with
# other front ends, but can't due to cmake issues
# then again, the main file should be split up into separate dirs anyway

set(
    SRC_WX
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
    # icon File
    xrc/vbam.xpm
    ../common/version.cpp
)

if(APPLE)
    set(SRC_WX ${SRC_WX} macsupport.mm)
endif()

set(
    HDR_WX
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
    ../common/version_cpp.h
)

set(
    RES_WX
    ${XRC_SOURCES}
    ../vba-over.ini
)

set(
    CM_STUFF
    copy-events.cmake
)

if(ENABLE_OPENAL)
    set(SRC_WX ${SRC_WX} openal.cpp)
    set(HDR_WX ${HDR_WX} openal.h)
endif()

if(ENABLE_XAUDIO2)
    set(SRC_WX ${SRC_WX} xaudio2.cpp)
endif()

if(ENABLE_FAUDIO)
    set(SRC_WX ${SRC_WX} faudio.cpp)
endif()

if(WIN32)
    set(SRC_WX ${SRC_WX} wxvbam.rc dsound.cpp)
    set(DIRECTX_LIBRARIES dxguid dsound ws2_32)
    if(MSVC)
        # workaround for some symbols needed by static SDL2.lib
        set(DIRECTX_LIBRARIES ${DIRECTX_LIBRARIES} imm32 version)
    endif()
    # not strictly directx, but win32-related
    if(ENABLE_DEBUGGER)
        set(DIRECTX_LIBRARIES ${DIRECTX_LIBRARIES} wsock32)
    endif()
endif()

link_directories(${CMAKE_BINARY_DIR})

set(VBAM_ICON vbam.icns)

set(VBAM_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/icons/${VBAM_ICON})

if(APPLE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
endif()

add_executable(
    visualboyadvance-m
    WIN32
    MACOSX_BUNDLE
    ${SRC_WX}
    ${HDR_WX}
    ${RES_WX}
    ${VBAM_ICON_PATH}
    ${CM_STUFF}
)


target_link_libraries(
    visualboyadvance-m
    ${VBAMCORE_LIBS}
    ${wxWidgets_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${DIRECTX_LIBRARIES}
    ${GTK_LIBRARIES}
    ${OPENAL_LIBRARY}
    ${FAUDIO_LIBS}
    ${EXTRA_X11_LIBS}
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
        if(CMAKE_BUILD_TYPE MATCHES Debug)
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

if(WIN32)
    install(PROGRAMS ${PROJECT_BINARY_DIR}/visualboyadvance-m${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${CMAKE_BINARY_DIR})
endif()

if(NOT WIN32 AND NOT APPLE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/wxvbam.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
endif()

if(APPLE)
    # this should set ROM file types correctly
    set_property(TARGET visualboyadvance-m APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
    set(MACOSX_BUNDLE_ICON_FILE ${VBAM_ICON})
    set_source_files_properties(${VBAM_ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # bundle dylibs and relink them for releasing .app
    # also install translations into the .app
    # but only in Release mode
    if(NOT CMAKE_BUILD_TYPE MATCHES Debug)
        add_custom_command(
            TARGET visualboyadvance-m POST_BUILD
            COMMAND ${CMAKE_SOURCE_DIR}/tools/osx/third_party_libs_tool ./visualboyadvance-m.app
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        )

        add_custom_command(
            TARGET visualboyadvance-m POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/mac-translations.cmake
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()
endif()

set(WX_EXE_NAME visualboyadvance-m-wx${CMAKE_EXECUTABLE_SUFFIX})
