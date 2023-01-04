if(WIN32 AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
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

    find_program(WXRC NAMES wxrc)

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
    if(NOT VCPKG_TARGET_TRIPLET MATCHES "-static")
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
#include <wx/wx.h>
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