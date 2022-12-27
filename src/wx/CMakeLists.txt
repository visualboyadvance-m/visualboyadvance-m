# Do not use this file directly.  Always use the top level CMakeLists.txt file

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW) # link to full path of libs
    cmake_policy(SET CMP0005 NEW) # escapes in add_definitions


    if(POLICY CMP0074)
        cmake_policy(SET CMP0074 NEW) # use Package_ROOT if set
    endif()

    if(POLICY CMP0077)
        cmake_policy(SET CMP0077 NEW) # use vars for options
    endif()

    if(POLICY CMP0043)
        cmake_policy(SET CMP0043 NEW) # for wxWidgets, use generator expressions
    endif()
endif()

include(VbamFunctions)

set(VBAM_LIBS ${VBAMCORE_LIBS})

if(WIN32)
    # not yet implemented
    option(ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" OFF)

    option(ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ON)
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

        list(APPEND VBAM_LIBS ${OPENAL_LIBRARY})
else()
    add_definitions(-DNO_OAL)
endif()

if(APPLE)
    add_definitions(-DwxMAC_USE_CORE_GRAPHICS)
endif()

if(NOT ENABLE_XAUDIO2)
    add_definitions(-DNO_XAUDIO2)
endif()

if(NOT ENABLE_DIRECT3D)
    add_definitions(-DNO_D3D)
endif()

if(ENABLE_FAUDIO)
    find_package(FAudio REQUIRED)
    list(APPEND VBAM_LIBS FAudio)
else()
    add_definitions(-DNO_FAUDIO)
endif()

# on unix we have to check for X11 before we overwrite all the compile/link
# flags with the wx tests
if(NOT WIN32 AND NOT APPLE)
    find_package(X11)

    if(X11_X11_LIB)
        include_directories(${X11_INCLUDE_DIR})
        list(APPEND VBAM_LIBS ${X11_X11_LIB})
    endif()
    if(X11_Xscreensaver_LIB)
        list(APPEND VBAM_LIBS ${X11_Xscreensaver_LIB})
        add_definitions(-DHAVE_XSS)
    endif()

    find_library(EGL_LIBRARY EGL)

    if(EGL_LIBRARY)
        list(APPEND VBAM_LIBS ${EGL_LIBRARY})
        add_definitions(-DHAVE_EGL)
    endif()

    find_library(WAYLAND_LIBRARY wayland-client)

    if(WAYLAND_LIBRARY)
        list(APPEND VBAM_LIBS ${WAYLAND_LIBRARY})
    endif()
endif()

# Win32 definitions common to all toolchains.
if (WIN32)
    add_definitions(-D_UNICODE -DUNICODE -DwxUSE_GUI=1 -D__WXMSW__ -DWINVER=0x0A00 -DNTDDI_VERSION=0x0A000007)
endif()

# on VS with vcpkg we can't use FindwxWidgets, we have to set everything up
# manually because the package is broken
if(WIN32 AND CMAKE_TOOLCHAIN_FILE MATCHES vcpkg)
    # set up wxwidgets stuff
    set(libtype u)
    unset(arch_suffix)
    unset(path_prefix)
    set(build_suffix -rel)

    if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
        set(libtype ud)
        set(path_prefix debug)
        set(build_suffix -dbg)

        add_definitions(-D_DEBUG)
    endif()

    set(build_suffix_rel -rel)
    set(build_suffix_dbg -dbg)

    if(VCPKG_TARGET_TRIPLET MATCHES -static)
        set(arch_suffix  -static)

        set(build_suffix_rel -static-rel)
        set(build_suffix_dbg -static-dbg)

        set(build_suffix -static${build_suffix})
    else()
        add_definitions(-DWXUSINGDLL)
    endif()

    set(common_prefix    ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows${arch_suffix})
    set(dbg_prefix       ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows${arch_suffix}/debug)
    set(installed_prefix ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows${arch_suffix}/${path_prefix})

    include_directories(${installed_prefix}/lib/msw${libtype})
    include_directories(${common_prefix}/include)
    set(wxWidgets_LIB_DIR ${installed_prefix}/lib)

    foreach(wx_lib
        ${wxWidgets_LIB_DIR}/wxbase*${libtype}_net.lib
        ${wxWidgets_LIB_DIR}/wxbase*${libtype}_xml.lib
        ${wxWidgets_LIB_DIR}/wxmsw*${libtype}_core.lib
        ${wxWidgets_LIB_DIR}/wxmsw*${libtype}_gl.lib
        ${wxWidgets_LIB_DIR}/wxmsw*${libtype}_xrc.lib
        ${wxWidgets_LIB_DIR}/wxmsw*${libtype}_html.lib
        ${wxWidgets_LIB_DIR}/wxbase*${libtype}.lib
    )
        file(GLOB wx_lib_file "${wx_lib}")

        if(wx_lib_file)
            list(APPEND wxWidgets_LIBRARIES "${wx_lib_file}")
        endif()
    endforeach()

    list(
        APPEND wxWidgets_LIBRARIES
        winmm comctl32 oleacc rpcrt4 shlwapi version wsock32 opengl32
    )

    if(EXISTS ${wxWidgets_LIB_DIR}/wxregex${libtype}.lib)
        list(APPEND wxWidgets_LIBRARIES ${wxWidgets_LIB_DIR}/wxregex${libtype}.lib)
    endif()

    if(VCPKG_TARGET_TRIPLET MATCHES -static)
        unset(deb_suffix)
        if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
            set(deb_suffix d)
        endif()

        foreach(lib_name libpng jpeg lzma tiff libexpat pcre2)
            file(
                GLOB lib_file
                ${wxWidgets_LIB_DIR}/${lib_name}*.lib
            )

            if(lib_file)
                list(APPEND wxWidgets_LIBRARIES ${lib_file})
            endif()
        endforeach()
    endif()

    set(WXRC ${common_prefix}/tools/wxwidgets/wxrc.exe)

    set(ENV{PATH} "${dbg_prefix}/bin;${common_prefix}/bin;$ENV{PATH}")

    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/zip.exe)
        # get zip binary for wxrc
        file(DOWNLOAD "https://www.willus.com/archive/zip64/infozip_binaries_win32.zip" ${CMAKE_CURRENT_BINARY_DIR}/infozip_binaries_win32.zip)
        # unzip it
        execute_process(
            COMMAND powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::ExtractToDirectory('${CMAKE_CURRENT_BINARY_DIR}/infozip_binaries_win32.zip', '${CMAKE_CURRENT_BINARY_DIR}'); }"
        )

        set(ZIP_PROGRAM ${CMAKE_CURRENT_BINARY_DIR}/zip.exe CACHE STRING "zip compressor executable" FORCE)
    endif()

    # SDL2.dll does not get copied to build dir
    if(NOT VCPKG_TARGET_TRIPLET MATCHES -static)
        unset(deb_suffix)
        if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
            set(deb_suffix d)
        endif()

        set(dll_path ${installed_prefix}/bin/SDL2${deb_suffix}.dll)

        if(NOT EXISTS ${CMAKE_BINARY_DIR}/SDL2${deb_suffix}.dll AND EXISTS ${dll_path})
            file(COPY ${dll_path} DESTINATION ${CMAKE_BINARY_DIR})
        endif()
    endif()
else()
    if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
        set(wxWidgets_USE_DEBUG ON) # noop if wx is compiled with --disable-debug, like in Mac Homebrew atm
    endif()

    # on e.g. msys2 add a couple of libraries wx needs
    #if(WIN32 AND (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang))
    #    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -luuid -lwinspool")
    #endif()

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

    if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
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
        AND NOT CMAKE_SYSTEM_NAME STREQUAL FreeBSD
        AND NOT TRANSLATIONS_ONLY)

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
    if (MH_CreateHook(reinterpret_cast<LPVOID>(&MessageBoxW), reinterpret_cast<LPVOID>(&DetourMessageBoxW), reinterpret_cast<LPVOID*>(&fpMessageBoxW)) != MH_OK)
        return 1;
    if (MH_CreateHook(reinterpret_cast<LPVOID>(&MessageBoxA), reinterpret_cast<LPVOID>(&DetourMessageBoxA), reinterpret_cast<LPVOID*>(&fpMessageBoxA)) != MH_OK)
        return 1;

    if (MH_EnableHook(reinterpret_cast<LPVOID>(&MessageBoxW)) != MH_OK) return 1;
    if (MH_EnableHook(reinterpret_cast<LPVOID>(&MessageBoxA)) != MH_OK) return 1;
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

            if(X86_64)
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
                string(REGEX REPLACE "<FLAGS>" "<FLAGS> -fabi-version=${WX_ABI_VERSION} " CMAKE_CXX_COMPILE_OBJECT "${CMAKE_CXX_COMPILE_OBJECT}")
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
            list(APPEND VBAM_LIBS ${GTK4_LIBRARIES})
        elseif(WX_USING_GTK3)
            pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
            if(NOT GTK3_INCLUDE_DIRS)
                message(FATAL_ERROR "Could not find gtk3")
            endif()
            include_directories(${GTK3_INCLUDE_DIRS})
            link_directories(${GTK3_LIBRARY_DIRS})
            add_compile_options(${GTK3_CFLAGS_OTHER})
            list(APPEND VBAM_LIBS ${GTK3_LIBRARIES})
        else()
            check_cxx_symbol_exists(__WXGTK20__ ${WX_CONFIG_H} WX_USING_GTK2)
            if(WX_USING_GTK2)
                # try to use pkg-config to find gtk2 first
                pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
                if(GTK2_INCLUDE_DIRS)
                    include_directories(${GTK2_INCLUDE_DIRS})
                    link_directories(${GTK2_LIBRARY_DIRS})
                    add_compile_options(${GTK2_CFLAGS_OTHER})
                    list(APPEND VBAM_LIBS ${GTK2_LIBRARIES})
                else()
                    # and if that fails, use the cmake module
                    find_package(GTK2 REQUIRED gtk)
                    if(NOT GTK2_INCLUDE_DIRS)
                        message(FATAL_ERROR "Could not find gtk2")
                    endif()
                    include_directories(${GTK2_INCLUDE_DIRS})
                    add_compile_options(${GTK2_DEFINITIONS})
                    list(APPEND VBAM_LIBS ${GTK2_LIBRARIES})
                endif()
            else()
                find_package(GTK REQUIRED gtk)
                if(NOT GTK_INCLUDE_DIRS)
                    message(FATAL_ERROR "Could not find gtk")
                endif()
                include_directories(${GTK_INCLUDE_DIRS})
                add_compile_options(${GTK_DEFINITIONS})
                list(APPEND VBAM_LIBS ${GTK_LIBRARIES})
            endif()
        endif()
    endif()

    if(wxWidgets_USE_FILE)
        include(${wxWidgets_USE_FILE})
    endif()
endif() # wxWidgets checks

if(ENABLE_NLS)
    if(MSVC)
        # Install gettext tools from nuget.

        # First fetch the nuget binary.
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/nuget.exe)
            file(DOWNLOAD "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" ${CMAKE_BINARY_DIR}/nuget.exe)
        endif()

        # Add nuget package source.
        execute_process(
            COMMAND nuget sources add -Name "NuGet official package source" -Source "https://api.nuget.org/v3/index.json"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        # Install the Gettext.Tools package.
        execute_process(
            COMMAND nuget.exe install Gettext.Tools -OutputDirectory ${CMAKE_BINARY_DIR}/nuget
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        # Find the path to the binaries in the package and add them to find path.
        file(GLOB pkg ${CMAKE_BINARY_DIR}/nuget/Gettext.Tools*)

        list(APPEND CMAKE_PROGRAM_PATH ${pkg}/tools/bin)
    endif()

    find_package(Gettext REQUIRED)

    find_program(XGETTEXT xgettext)
    find_program(MSGINIT  msginit)

    if(NOT XGETTEXT OR NOT MSGINIT)
        message(SEND_ERROR "Cannot find gettext xgettext:'${XGETTEXT}' msginit:'${MSGINIT}'")
    endif()
endif()

# contrib widgets
include_directories(widgets)

# for out-of-tree builds, grab includes from both target and source dirs
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_program(ZIP_PROGRAM zip DOC "zip compressor executable")

if(NOT ZIP_PROGRAM)
    message(FATAL_ERROR "The zip compressor program is required for building.")
endif()

set(
    XRC_SOURCES
    xrc/AccelConfig.xrc
    xrc/CheatAdd.xrc
    xrc/CheatCreate.xrc
    xrc/CheatEdit.xrc
    xrc/CheatList.xrc
    xrc/CodeSelect.xrc
    xrc/DirectoriesConfig.xrc
    xrc/Disassemble.xrc
    xrc/DisplayConfig.xrc
    xrc/ExportSPS.xrc
    xrc/GBAROMInfo.xrc
    xrc/GBColorPrefPanel.xrc
    xrc/GBDisassemble.xrc
    xrc/GBMapViewer.xrc
    xrc/GBOAMViewer.xrc
    xrc/GBPaletteViewer.xrc
    xrc/GBPrinter.xrc
    xrc/GBROMInfo.xrc
    xrc/GBTileViewer.xrc
    xrc/GameBoyAdvanceConfig.xrc
    xrc/GameBoyConfig.xrc
    xrc/GeneralConfig.xrc
    xrc/IOViewer.xrc
    xrc/JoyPanel.xrc
    xrc/JoypadConfig.xrc
    xrc/LinkConfig.xrc
    xrc/Logging.xrc
    xrc/MainFrame.xrc
    xrc/MainIcon.xrc
    xrc/MainMenu.xrc
    xrc/MapViewer.xrc
    xrc/MemSelRegion.xrc
    xrc/MemViewer.xrc
    xrc/NetLink.xrc
    xrc/OAMViewer.xrc
    xrc/PaletteViewer.xrc
    xrc/SoundConfig.xrc
    xrc/TileViewer.xrc
    xrc/SpeedupConfig.xrc
    xrc/UIConfig.xrc
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

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs
    COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH}" ${WXRC} ${XRC_SOURCES} -o ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
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
    background-input.cpp
    guiinit.cpp
    viewers.cpp
    gfxviewers.cpp
    cmdevents.cpp
    extra-translations.cpp
    opts.cpp
    sys.cpp
    panel.cpp
    viewsupt.cpp
    wayland.cpp
    strutils.cpp
    wxutil.cpp
    config/game-control.cpp
    config/internal/option-internal.cpp
    config/option-observer.cpp
    config/option.cpp
    config/user-input.cpp
    dialogs/display-config.cpp
    widgets/keyedit.cpp
    widgets/joyedit.cpp
    widgets/option-validator.cpp
    widgets/render-plugin.cpp
    widgets/sdljoy.cpp
    widgets/wxmisc.cpp
    # probably ought to be in common
    ../sdl/text.cpp
    # from external source with minor modifications
    widgets/checkedlistctrl.cpp
    ../common/version.cpp
)

set(ALL_SRC_WX ${SRC_WX})

list(APPEND ALL_SRC_WX ${CMAKE_CURRENT_SOURCE_DIR}/macsupport.mm)
list(APPEND ALL_SRC_WX ${CMAKE_CURRENT_SOURCE_DIR}/widgets/dpi-support-mac.mm)
list(APPEND ALL_SRC_WX ${CMAKE_CURRENT_SOURCE_DIR}/widgets/dpi-support.cpp)

if(APPLE)
    list(APPEND SRC_WX macsupport.mm)
    list(APPEND SRC_WX widgets/dpi-support-mac.mm)
else()
    list(APPEND SRC_WX widgets/dpi-support.cpp)
endif()

set(
    HDR_WX
    wxvbam.h
    background-input.h
    wxlogdebug.h
    drawing.h
    filters.h
    ioregs.h
    opts.h
    viewsupt.h
    wxhead.h
    wayland.h
    wxutil.h
    config/game-control.h
    config/internal/option-internal.h
    config/option-id.h
    config/option-observer.h
    config/option.h
    config/user-input.h
    dialogs/display-config.h
    widgets/dpi-support.h
    widgets/option-validator.h
    widgets/render-plugin.h
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
    ../common/range.hpp
    ../common/contains.h
)

set(ALL_HDR_WX ${HDR_WX})
# make files included for gettext pot generation
list(APPEND ALL_SRC_WX autoupdater/wxmsw/autoupdater.cpp)
list(APPEND ALL_HDR_WX autoupdater/autoupdater.h)
list(APPEND ALL_SRC_WX autoupdater/wxmsw/winsparkle-wrapper.cpp)
list(APPEND ALL_HDR_WX autoupdater/wxmsw/winsparkle-wrapper.h)
list(APPEND ALL_HDR_WX autoupdater/wxmsw/winsparkle-rc.h)

if(WIN32 AND (X86_64 OR X86_32) AND ENABLE_ONLINEUPDATES)
    list(APPEND SRC_WX autoupdater/wxmsw/autoupdater.cpp)
    list(APPEND HDR_WX autoupdater/autoupdater.h)
    list(APPEND SRC_WX autoupdater/wxmsw/winsparkle-wrapper.cpp)
    list(APPEND HDR_WX autoupdater/wxmsw/winsparkle-wrapper.h)
    list(APPEND HDR_WX autoupdater/wxmsw/winsparkle-rc.h)
endif()

# make files included for gettext pot generation
list(APPEND ALL_HDR_WX autoupdater/autoupdater.h)
list(APPEND ALL_SRC_WX autoupdater/macos/autoupdater.cpp)
list(APPEND ALL_SRC_WX autoupdater/macos/sparkle-wrapper.mm)
list(APPEND ALL_HDR_WX autoupdater/macos/sparkle-wrapper.h)

if(APPLE AND ENABLE_ONLINEUPDATES)
    list(APPEND HDR_WX autoupdater/autoupdater.h)
    list(APPEND SRC_WX autoupdater/macos/autoupdater.cpp)
    list(APPEND SRC_WX autoupdater/macos/sparkle-wrapper.mm)
    list(APPEND HDR_WX autoupdater/macos/sparkle-wrapper.h)
endif()

set(
    RES_WX
    ${XRC_SOURCES}
    ../vba-over.ini
    # icon File
    xrc/visualboyadvance-m.xpm
    # generated includes must be explicitly listed
    ${CMAKE_CURRENT_BINARY_DIR}/builtin-xrc.h
    ${CMAKE_CURRENT_BINARY_DIR}/builtin-over.h
    ${CMAKE_CURRENT_BINARY_DIR}/cmdhandlers.h
    ${CMAKE_CURRENT_BINARY_DIR}/cmd-evtable.h
    # generated
    ${CMAKE_CURRENT_BINARY_DIR}/cmdtab.cpp
)

set(
    CM_STUFF
    copy-events.cmake
)

list(APPEND ALL_SRC_WX openal.cpp)
list(APPEND ALL_HDR_WX openal.h)

if(ENABLE_OPENAL)
   list(APPEND SRC_WX openal.cpp)
   list(APPEND HDR_WX openal.h)
endif()

list(APPEND ALL_SRC_WX xaudio2.cpp)

if(ENABLE_XAUDIO2)
   list(APPEND SRC_WX xaudio2.cpp)
endif()

list(APPEND ALL_SRC_WX faudio.cpp)

if(ENABLE_FAUDIO)
   list(APPEND SRC_WX faudio.cpp)
endif()

list(APPEND ALL_SRC_WX dsound.cpp)

if(WIN32)
    list(APPEND SRC_WX dsound.cpp)

    list(APPEND RES_WX wxvbam.rc)

    list(APPEND VBAM_LIBS dxguid dsound wsock32 ws2_32 imm32 version)
endif()

link_directories(${CMAKE_BINARY_DIR})

set(VBAM_ICON visualboyadvance-m.icns)

set(VBAM_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/icons/${VBAM_ICON})

if(APPLE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
endif()

if(NOT TRANSLATIONS_ONLY)
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

    if(WIN32 AND (X86_64 OR X86_32) AND ENABLE_ONLINEUPDATES)
        if(NOT DEFINED WINSPARKLE_BIN_RELEASE_DIR)
            set(WINSPARKLE_BIN_RELEASE_DIR ${CMAKE_SOURCE_DIR}/dependencies/WinSparkle-0.6.0)
        endif()

        target_include_directories(
            visualboyadvance-m
            PRIVATE ${WINSPARKLE_BIN_RELEASE_DIR}/include
        )

        if(X86_64)
            set(WINSPARKLE_DLL ${CMAKE_SOURCE_DIR}/dependencies/WinSparkle-0.6.0/x64/Release/WinSparkle.dll)
        else()
            set(WINSPARKLE_DLL ${CMAKE_SOURCE_DIR}/dependencies/WinSparkle-0.6.0/Release/WinSparkle.dll)
        endif()

        configure_file(autoupdater/wxmsw/winsparkle-path.h.in ${CMAKE_BINARY_DIR}/winsparkle-path.h)
    endif()

    if(APPLE AND ENABLE_ONLINEUPDATES)
        include(FetchContent)
        FetchContent_Declare(Sparkle
            URL "https://github.com/sparkle-project/Sparkle/releases/download/2.3.0-beta.2/Sparkle-2.3.0-beta.2.tar.xz"
            URL_HASH SHA256=6875388aae23c1705c3956c62a9c967f4b788bc4f1dad93ab5645bc6096ef13b
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(Sparkle)

        find_library(SPARKLE_FRAMEWORK
          NAMES Sparkle
          HINTS ${sparkle_SOURCE_DIR}
        )
        find_path(SPARKLE_INCLUDE_DIR Sparkle.h HINTS ${SPARKLE_FRAMEWORK}/Headers)
        target_include_directories(
            visualboyadvance-m
            PRIVATE ${SPARKLE_INCLUDE_DIR}
        )

        set(APPCAST_URL "https://data.vba-m.com/appcast.xml")
        set(CCS1 rm -rf ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/Sparkle.framework)
        set(CCS2 mkdir -p ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks)
        # The following commands *should* be run to make sure Sparkle is not going
        # to bug randomly.
        set(CCS3 cp -a ${SPARKLE_FRAMEWORK} ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/Sparkle.framework)
        set(CCS4 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist CFBundleVersion            -string "${VERSION}")
        set(CCS5 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist CFBundleShortVersionString -string "${VERSION}")
        set(CCS6 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUEnableAutomaticChecks -bool YES)
        #set(CCS7 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUPublicEDKey -string "${PUBLIC_KEY}")
        set(CCS8 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUFeedURL -string "${APPCAST_URL}")
        add_custom_command(TARGET visualboyadvance-m POST_BUILD
          COMMAND ${CCS1}
          COMMAND ${CCS2}
          COMMAND ${CCS3}
          COMMAND ${CCS4}
          COMMAND ${CCS5}
          COMMAND ${CCS6}
          #COMMAND ${CCS7}
          COMMAND ${CCS8}
        )

        TARGET_LINK_LIBRARIES(visualboyadvance-m ${SPARKLE_FRAMEWORK})
    endif()

    target_link_libraries(
        visualboyadvance-m
        ${wxWidgets_LIBRARIES}
        ${VBAM_LIBS}
    )

    if(ENABLE_FFMPEG)
        target_link_libraries(
            visualboyadvance-m
            ${FFMPEG_LIBRARIES}
        )

        if(FFMPEG_LDFLAGS)
            join("${FFMPEG_LDFLAGS}" " " FFMPEG_LDFLAGS_STR)

            set_target_properties(
                visualboyadvance-m
                PROPERTIES LINK_FLAGS ${FFMPEG_LDFLAGS_STR}
            )
        endif()
    endif()

    if(WIN32)
        # Force a re-link when the manifest file is modified.
        set_target_properties(visualboyadvance-m
            PROPERTIES
            LINK_DEPENDS
            "${CMAKE_CURRENT_LIST_DIR}/visualboyadvance-m.manifest")

        if(MSVC)
            # Disable the auto-generated manifest from CMake.
            target_link_options(visualboyadvance-m PRIVATE "/MANIFEST:NO")
        endif()
    endif()

    # link libgcc/libstdc++ statically on mingw
    # and adjust link command when making a static binary
    if(CMAKE_COMPILER_IS_GNUCXX AND VBAM_STATIC)
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

    if(NOT WIN32 AND NOT APPLE)
        install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/visualboyadvance-m.desktop     DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
        install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/visualboyadvance-m.appdata.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/appdata)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/icons/sizes/                   DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor)
    endif()
endif()

option(TRANSLATIONS_ONLY "Build only the translations.zip" OFF)

# Make the translations.zip
if(ENABLE_NLS)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/translations.zip
        COMMAND ${CMAKE_COMMAND} -D "ZIP_PROGRAM=${ZIP_PROGRAM}" -P ${CMAKE_CURRENT_SOURCE_DIR}/make-translations-zip.cmake
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS translations make-translations-zip.cmake
    )

    add_custom_target(
        translations-zip
        SOURCES ${CMAKE_BINARY_DIR}/translations.zip
    )

    add_dependencies(visualboyadvance-m translations-zip)
endif()

# Update the gettext pot source.
# Do this automatically instead of manually to make sure we don't forget to update.
if(ENABLE_NLS)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH}" ${WXRC} -g ${XRC_SOURCES} -o ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${XRC_SOURCES}
    )

    add_custom_target(xrc-strings DEPENDS ${CMAKE_BINARY_DIR}/wx-xrc-strings.h)

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/wxvbam.pot
        COMMAND ${XGETTEXT} -k_ -kN_ -o ${CMAKE_BINARY_DIR}/wxvbam.pot ${ALL_SRC_WX} ${ALL_HDR_WX} ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        DEPENDS ${ALL_SRC_WX} ${ALL_HDR_WX} ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_custom_target(gettext-pot ALL DEPENDS ${CMAKE_BINARY_DIR}/wxvbam.pot)

    add_dependencies(gettext-pot xrc-strings)

    add_custom_command(
        TARGET gettext-pot
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -D SRC_DIR=${CMAKE_SOURCE_DIR}/po/wxvbam -D BIN_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/check-pot-updated.cmake
    )
endif()

if(NOT TRANSLATIONS_ONLY)
    find_program(GPG_PROGRAM gpg)

    if(GPG_PROGRAM)
        execute_process(
            COMMAND ${GPG_PROGRAM} -k
            OUTPUT_VARIABLE GPG_KEYS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    option(ZIP_SUFFIX [=[suffix for release zip files, e.g.  "-somebranch".zip]=] OFF)

    if(NOT ZIP_SUFFIX)
        set(ZIP_SUFFIX "")
    endif()

    if(UPSTREAM_RELEASE AND WIN32)
        set(home "$ENV{HOME}")

        if(NOT CMAKE_CROSSCOMPILING AND NOT DEFINED ENV{MSYSTEM_PREFIX})
            set(home "$ENV{USERPROFILE}")
        endif()

        # rewrite backslashes to slashes, needed for msys osslsigncode
        string(REGEX REPLACE "\\\\" "/" home "${home}")

        set(cert "${home}/.codesign/windows_comodo.pkcs12")

        if(EXISTS "${cert}")
            find_program(OSSLSIGNCODE_PROGRAM osslsigncode)
            find_program(SIGNTOOL_PROGRAM     signtool)

            if(OSSLSIGNCODE_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E rename visualboyadvance-m.exe visualboyadvance-m-unsigned.exe
                    COMMAND ${OSSLSIGNCODE_PROGRAM} sign -pkcs12 ${cert} -pass "vbam3!13" -t http://timestamp.digicert.com -n visualboyadvance-m -i https://github.com/visualboyadvance-m/visualboyadvance-m -in visualboyadvance-m-unsigned.exe -out visualboyadvance-m.exe
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    )
            elseif(SIGNTOOL_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy visualboyadvance-m.exe visualboyadvance-m-unsigned.exe
                    COMMAND ${SIGNTOOL_PROGRAM} sign /f ${cert} /p "vbam3!13" /tr http://timestamp.digicert.com /du https://github.com/visualboyadvance-m/visualboyadvance-m /a visualboyadvance-m.exe
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    )
            endif()
        endif()

        if(CMAKE_BUILD_TYPE STREQUAL Debug)
            set(exezip visualboyadvance-m-Win-${ARCH_NAME}-debug${ZIP_SUFFIX}.zip)
        else()
            set(exezip visualboyadvance-m-Win-${ARCH_NAME}${ZIP_SUFFIX}.zip)
        endif()

        unset(pdb_file)

        if(MSVC AND CMAKE_BUILD_TYPE STREQUAL Debug)
            set(pdb_file visualboyadvance-m.pdb)
        endif()

        add_custom_command(
            TARGET visualboyadvance-m
            POST_BUILD
            COMMAND ${ZIP_PROGRAM} -9 ${exezip} visualboyadvance-m.exe ${pdb_file}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        if(GPG_KEYS)
            add_custom_command(
                OUTPUT ${CMAKE_BINARY_DIR}/translations.zip.asc
                COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/translations.zip.asc
    #            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/interactive-pause.cmake
                COMMAND ${GPG_PROGRAM} --detach-sign -a ${CMAKE_BINARY_DIR}/translations.zip
                DEPENDS ${CMAKE_BINARY_DIR}/translations.zip
            )

            add_custom_target(translations-zip-sig DEPENDS ${CMAKE_BINARY_DIR}/translations.zip.asc)

            add_dependencies(translations-zip translations-zip-sig)

            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove ${exezip}.asc
    #            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/interactive-pause.cmake
                COMMAND ${GPG_PROGRAM} --detach-sign -a ${exezip}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()

        if(NOT MSVC AND CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
            find_program(STRIP_PROGRAM strip)

            if(STRIP_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${STRIP_PROGRAM} visualboyadvance-m.exe
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endif()
        endif()
    endif()

    if(APPLE)
        # this should set ROM file types correctly
        set_property(TARGET visualboyadvance-m APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
        set(MACOSX_BUNDLE_ICON_FILE ${VBAM_ICON})
        set_source_files_properties(${VBAM_ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

        # bundle dylibs and relink them for releasing .app
        # also install translations into the .app
        # but only in Release mode
        if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
            add_custom_command(
                TARGET visualboyadvance-m POST_BUILD
                COMMAND ${CMAKE_SOURCE_DIR}/tools/macOS/third_party_libs_tool ./visualboyadvance-m.app
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            )

            add_custom_command(
                TARGET visualboyadvance-m POST_BUILD
                COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/mac-translations.cmake
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    endif()

    if(APPLE AND (UPSTREAM_RELEASE OR ENABLE_ONLINEUPDATES))
        if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
            find_program(STRIP_PROGRAM strip)

            if(STRIP_PROGRAM)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${STRIP_PROGRAM} visualboyadvance-m.app/Contents/MacOS/visualboyadvance-m
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endif()
        endif()

        add_custom_command(
            TARGET visualboyadvance-m
            POST_BUILD
            VERBATIM COMMAND sh -c [=[codesign --sign "Developer ID Application" --force --deep ./visualboyadvance-m.app || :]=]
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        if(EXISTS ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks)
            # Sign frameworks individually, like Xcode.
            file(GLOB frameworks ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/*)
            foreach(framework ${frameworks})
                message(STATUS "Signing framework: " ${framework})
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    VERBATIM COMMAND sh -c "codesign --sign 'Developer ID Application' --force ${framework} || :"
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endforeach()
        endif()

        if(UPSTREAM_RELEASE)
           if(CMAKE_BUILD_TYPE STREQUAL Debug)
              set(appzip visualboyadvance-m-Mac-${ARCH_NAME}-debug${ZIP_SUFFIX}.zip)
           else()
              set(appzip visualboyadvance-m-Mac-${ARCH_NAME}${ZIP_SUFFIX}.zip)
           endif()

            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove ${appzip}
                COMMAND ${ZIP_PROGRAM} -9yr ${appzip} ./visualboyadvance-m.app
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )

            if(GPG_KEYS)
                add_custom_command(
                    TARGET visualboyadvance-m
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E remove ${appzip}.asc
    #                COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/interactive-pause.cmake
                    COMMAND ${GPG_PROGRAM} --detach-sign -a ${appzip}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endif()
        endif()
    endif()

    if(UPSTREAM_RELEASE AND NOT WIN32 AND NOT APPLE AND CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
        find_program(STRIP_PROGRAM strip)

        if(STRIP_PROGRAM)
            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${STRIP_PROGRAM} visualboyadvance-m
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    endif()

    install(
        TARGETS visualboyadvance-m
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    if(BUILD_TESTING AND (NOT CMAKE_CROSSCOMPILING))
        add_subdirectory(tests)
    endif()
endif()

# vim:sw=4 et tw=0:
