if(NOT TRANSLATIONS_ONLY AND NOT ENABLE_WX)
    return()
endif()

# Do not use this file directly.  Always use the top level CMakeLists.txt file
include(VbamFunctions)

set(VBAM_WX_COMMON
    audio/audio.cpp
    audio/audio.h
    audio/internal/coreaudio.cpp
    audio/internal/sdl.cpp
    audio/internal/sdl.h
    background-input.cpp
    background-input.h
    cmdevents.cpp
    dialogs/accel-config.cpp
    dialogs/accel-config.h
    dialogs/base-dialog.cpp
    dialogs/base-dialog.h
    dialogs/directories-config.cpp
    dialogs/directories-config.h
    dialogs/display-config.cpp
    dialogs/display-config.h
    dialogs/game-boy-config.cpp
    dialogs/game-boy-config.h
    dialogs/game-maker.cpp
    dialogs/game-maker.h
    dialogs/gb-rom-info.cpp
    dialogs/gb-rom-info.h
    dialogs/joypad-config.cpp
    dialogs/joypad-config.h
    dialogs/sound-config.cpp
    dialogs/sound-config.h
    dialogs/speedup-config.cpp
    dialogs/speedup-config.h
    drawing.h
    extra-translations.cpp
    gfxviewers.cpp
    guiinit.cpp
    ioregs.h
    opts.cpp
    opts.h
    panel.cpp
    rpi.h
    sys.cpp
    viewers.cpp
    viewsupt.cpp
    viewsupt.h
    wayland.cpp
    wayland.h
    wxhead.h
    wxlogdebug.h
    wxvbam.cpp
    wxvbam.h
    x11keymap.h
    xrc/visualboyadvance-m.xpm
    # Generated files.
    ${VBAM_GENERATED_DIR}/wx/builtin-xrc.h
    ${VBAM_GENERATED_DIR}/wx/builtin-over.h
    ${VBAM_GENERATED_DIR}/wx/cmdhandlers.h
    ${VBAM_GENERATED_DIR}/wx/cmd-evtable.h
    ${VBAM_GENERATED_DIR}/wx/gb-builtin-over.h
)

if(NOT ZIP_PROGRAM)
    find_program(ZIP_PROGRAM zip DOC "zip compressor executable")
    if(WIN32 AND NOT ZIP_PROGRAM)
        if(NOT DEFINED POWERSHELL)
            message(FATAL_ERROR "Powershell is required for zip file support when the `zip` program is not available.")
        endif()

        # Build zip.exe from zip.ps1 using ps12exe
        set(ZIP_EXE_DIR "${CMAKE_BINARY_DIR}/zip-bin")
        set(ZIP_EXE "${ZIP_EXE_DIR}/zip.exe")
        set(PS12EXE_DIR "${CMAKE_BINARY_DIR}/ps12exe")

        if(NOT EXISTS "${ZIP_EXE}")
            file(MAKE_DIRECTORY "${ZIP_EXE_DIR}")

            message(STATUS "Building zip.exe from zip.ps1 using ps12exe...")

            # Download ps12exe repository if not present
            if(NOT EXISTS "${PS12EXE_DIR}/ps12exe.ps1")
                message(STATUS "Downloading ps12exe...")
                file(DOWNLOAD
                    "https://github.com/steve02081504/ps12exe/archive/refs/heads/master.zip"
                    "${CMAKE_BINARY_DIR}/ps12exe.zip"
                    STATUS DOWNLOAD_STATUS
                )
                list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT)

                if(NOT DOWNLOAD_RESULT EQUAL 0)
                    message(FATAL_ERROR "Failed to download ps12exe. The zip program is required for building.")
                endif()

                # Extract the archive
                execute_process(
                    COMMAND ${POWERSHELL} -NoProfile -ExecutionPolicy Bypass -Command
                        "Expand-Archive -Path '${CMAKE_BINARY_DIR}/ps12exe.zip' -DestinationPath '${CMAKE_BINARY_DIR}' -Force"
                    RESULT_VARIABLE EXTRACT_RESULT
                )

                if(NOT EXTRACT_RESULT EQUAL 0)
                    message(FATAL_ERROR "Failed to extract ps12exe. The zip program is required for building.")
                endif()

                file(RENAME "${CMAKE_BINARY_DIR}/ps12exe-master" "${PS12EXE_DIR}")
            endif()

            execute_process(
                COMMAND ${POWERSHELL} -NoProfile -ExecutionPolicy Bypass -Command
                    "& '${PS12EXE_DIR}/ps12exe.ps1' -inputFile '${CMAKE_SOURCE_DIR}/src/wx/zip.ps1' -outputFile '${ZIP_EXE}' -noConsole:$false -architecture 'x86' -localize 'en'"
                RESULT_VARIABLE PS12EXE_BUILD_RESULT
            )

            if(NOT PS12EXE_BUILD_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to build zip.exe with ps12exe. The zip program is required for building.")
            endif()
        endif()

        set(ZIP_PROGRAM "${ZIP_EXE}")
        set(ENV{PATH} "${ZIP_EXE_DIR};$ENV{PATH}")
    endif()
    if(ZIP_PROGRAM)
        set(ZIP_PROGRAM "${ZIP_PROGRAM}" CACHE STRING "zip compressor executable" FORCE)
    else()
        message(FATAL_ERROR "The zip compressor program is required for building.")
    endif()
endif()

find_program(XGETTEXT xgettext)
find_program(MSGINIT  msginit)

if(CMAKE_HOST_WIN32 AND (NOT XGETTEXT OR NOT MSGINIT))
    # Install gettext tools from nuget.

    # First fetch the nuget binary.
    if(NOT EXISTS ${CMAKE_BINARY_DIR}/nuget.exe)
        file(DOWNLOAD "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" ${CMAKE_BINARY_DIR}/nuget.exe)
    endif()

    # Add nuget package source.
    execute_process(
        COMMAND nuget sources add -Name "NuGet official package source" -Source "https://api.nuget.org/v3/index.json"
        OUTPUT_QUIET
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Install the Gettext.Tools package.
    execute_process(
        COMMAND nuget.exe install Gettext.Tools -OutputDirectory ${CMAKE_BINARY_DIR}/nuget
        OUTPUT_QUIET
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Find the path to the binaries in the package and add them to find path.
    file(GLOB pkg ${CMAKE_BINARY_DIR}/nuget/Gettext.Tools*)

    list(APPEND CMAKE_PROGRAM_PATH ${pkg}/tools/bin)
endif()

unset(XGETTEXT)
unset(MSGINIT)

find_package(Gettext REQUIRED)
find_program(XGETTEXT xgettext)
find_program(MSGINIT  msginit)

if(NOT XGETTEXT OR NOT MSGINIT)
    message(SEND_ERROR "Cannot find gettext xgettext:'${XGETTEXT}' msginit:'${MSGINIT}'")
endif()

# Make the translations.zip
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/translations.zip
    COMMAND ${CMAKE_COMMAND} -D "ZIP_PROGRAM=${ZIP_PROGRAM}" -P ${CMAKE_CURRENT_SOURCE_DIR}/make-translations-zip.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS translations make-translations-zip.cmake
)
add_custom_target(
    translations-zip
    ALL
    SOURCES ${CMAKE_BINARY_DIR}/translations.zip
)

if(TRANSLATIONS_ONLY)
    # Nothing more to do if we are only building the translations.zip file.
    return()
endif()

# wxWidgets configuration.
set(wxWidgets_USE_UNICODE ON)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(wxWidgets_USE_DEBUG ON) # noop if wx is compiled with --disable-debug, like in Mac Homebrew atm
endif()
if(VBAM_STATIC)
    set(wxWidgets_USE_STATIC ON)
endif()

unset(wx_find_extra)
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
  set(wx_find_extra NO_DEFAULT_PATH)
  set(wxWidgets_DIR "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/share/wxwidgets")
  find_package(nanosvg)
endif()

if(APPLE)
    find_library(MAC_SYSTEM System)
    find_library(COREAUDIO CoreAudio)
    find_library(COREFOUNDATION CoreFoundation)
    find_library(AUDIOTOOLBOX AudioToolbox)
    find_library(METAL Metal)
    find_library(METALKIT MetalKit)
endif()

set(ENABLE_OPENGL TRUE)
find_package(wxWidgets COMPONENTS xrc xml html adv net core base gl ${wx_find_extra})

if(NOT wxWidgets_FOUND)
    find_package(wxWidgets COMPONENTS xrc xml html adv net core base ${wx_find_extra} REQUIRED)
    set(ENABLE_OPENGL FALSE)
endif()

# Fixup wxWidgets paths for vcpkg debug builds.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(wxWidgets_ROOT_DIR "${wxWidgets_ROOT_DIR}/debug" CACHE INTERNAL "wxWidgets root directory" FORCE)
    string(REGEX REPLACE "/lib$" "/debug/lib" wxWidgets_LIB_DIR "${wxWidgets_LIB_DIR}")
    set(wxWidgets_LIB_DIR "${wxWidgets_LIB_DIR}" CACHE INTERNAL "wxWidgets library directory" FORCE)
endif()

# Workaround of static liblzma not being found on MSYS2.
if(VBAM_STATIC AND MSYS)
    unset(cleaned_up_wx_libs)
    foreach(lib ${wxWidgets_LIBRARIES})
        if(lib STREQUAL "-llzma")
            set(lib "liblzma.a")
        endif()

        list(APPEND cleaned_up_wx_libs "${lib}")
    endforeach()

    set(wxWidgets_LIBRARIES "${cleaned_up_wx_libs}")
endif()

list(APPEND CMAKE_REQUIRED_LIBRARIES ${wxWidgets_LIBRARIES})
list(APPEND CMAKE_REQUIRED_INCLUDES ${wxWidgets_INCLUDE_DIRS})
list(APPEND CMAKE_REQUIRED_FLAGS ${wxWidgets_CXX_FLAGS})
foreach(DEF ${wxWidgets_DEFINITIONS})
    list(APPEND CMAKE_REQUIRED_DEFINITIONS "-D${DEF}")
endforeach()

# Configure common settings for wx-based targets, like linking, include
# directories, compile options, and definitions.
function(configure_wx_target target)
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "EXECUTABLE")
        set(target_is_executable TRUE)
    elseif(target_type STREQUAL "INTERFACE_LIBRARY")
        set(target_is_executable FALSE)
        set(target_is_interface_library TRUE)
    else()
        set(target_is_executable FALSE)
    endif()

    function(_add_link_libraries)
        if(${target_is_executable})
            target_link_libraries(${target} ${ARGN})
        elseif(${target_is_interface_library})
            target_link_libraries(${target} INTERFACE ${ARGN})
        else()
            target_link_libraries(${target} PUBLIC ${ARGN})
        endif()
    endfunction()

    function(_add_include_directories)
        if(${target_is_executable})
            target_include_directories(${target} PRIVATE ${ARGN})
        else()
            target_include_directories(${target} PUBLIC ${ARGN})
        endif()
    endfunction()

    function(_add_compile_options)
        if(${target_is_executable})
            target_compile_options(${target} PRIVATE ${ARGN})
        else()
            target_compile_options(${target} PUBLIC ${ARGN})
        endif()
    endfunction()

    function(_add_compile_definitions)
        if(${target_is_executable})
            target_compile_definitions(${target} PRIVATE ${ARGN})
        else()
            target_compile_definitions(${target} PUBLIC ${ARGN})
        endif()
    endfunction()

    # Core emulator.
    _add_link_libraries(vbam-core nonstd-lib ghc-filesystem)

    # wxWidgets.
    _add_link_libraries(${wxWidgets_LIBRARIES})
    _add_include_directories(${wxWidgets_INCLUDE_DIRS})
    _add_compile_options(${wxWidgets_CXX_FLAGS})
    _add_compile_definitions(${wxWidgets_DEFINITIONS})
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        _add_compile_definitions(${wxWidgets_DEFINITIONS_DEBUG})
    endif()

    # Generic file dialogs
    if (ENABLE_GENERIC_FILE_DIALOGS)
        _add_compile_definitions(USE_GENERIC_FILE_DIALOGS)
    endif()

    # No Metal
    if(NOT CMAKE_Metal_COMPILER)
        _add_compile_definitions(NO_METAL)
    endif()

    # OpenAL.
    if(ENABLE_OPENAL)
        _add_compile_definitions(VBAM_ENABLE_OPENAL)
        if(OPENAL_STATIC)
            _add_compile_definitions(AL_LIBTYPE_STATIC)
        endif()

        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            find_library(FMT_LIBRARY NAMES fmtd fmt)
        else()
            find_library(FMT_LIBRARY fmt)
        endif()

        if(FMT_LIBRARY)
            list(APPEND OPENAL_LIBRARY ${FMT_LIBRARY})
        endif()

        if(WIN32)
            list(APPEND OPENAL_LIBRARY avrt)
        endif()

        _add_include_directories(${OPENAL_INCLUDE_DIR})
        _add_link_libraries(${OPENAL_LIBRARY})
    endif()

    # XAudio2.
    if(ENABLE_XAUDIO2)
        _add_compile_definitions(VBAM_ENABLE_XAUDIO2)
    endif()

    # FAudio.
    if(ENABLE_FAUDIO)
        _add_compile_definitions(VBAM_ENABLE_FAUDIO)

        if(APPLE)
            _add_link_libraries(FAudio)
        else()
            if(VBAM_STATIC)
                _add_link_libraries(FAudio::FAudio-static)
            else()
                _add_link_libraries(FAudio::FAudio)
            endif()
        endif()

        if(WIN32)
            _add_link_libraries(dxguid uuid winmm ole32 advapi32 user32 mfplat mfreadwrite mfuuid propsys)
        endif()
    endif()

    # Direct3D.
    if(NOT ENABLE_DIRECT3D)
        _add_compile_definitions(NO_D3D)
    else()
        _add_link_libraries(d3d9)
    endif()

    # SDL.
    _add_link_libraries(${VBAM_SDL_LIBS})

    if(APPLE AND NOT NIX)
        _add_link_libraries(sharpyuv)
    endif()

    # OpenGL.
    if(ENABLE_OPENGL)
        _add_link_libraries(${OPENGL_LIBRARIES})
    else()
        _add_compile_definitions(NO_OGL)
    endif()

    # Metal
    if(APPLE)
        _add_link_libraries(${MAC_SYSTEM})
        _add_link_libraries(${COREFOUNDATION})
        _add_link_libraries(${COREAUDIO})
        _add_link_libraries(${AUDIOTOOLBOX})

        if(CMAKE_Metal_COMPILER)
            _add_link_libraries($<LINK_LIBRARY:WEAK_FRAMEWORK,${METAL}>)
            _add_link_libraries($<LINK_LIBRARY:WEAK_FRAMEWORK,${METALKIT}>)
        endif()
    endif()

endfunction()

# Sub-projects.
add_subdirectory(test)
add_subdirectory(config)
add_subdirectory(widgets)

set(VBAM_ICON visualboyadvance-m.icns)
set(VBAM_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/icons/${VBAM_ICON})

add_executable(
    visualboyadvance-m
    WIN32
    MACOSX_BUNDLE
)

if(APPLE)
    if(CMAKE_Metal_COMPILER)
        set(VBAM_SHADER default.metallib)

        set(ShaderBase_HEADERS
            AAPLShaderTypes.h
        )

        set(ShaderBase_SOURCES
            AAPLShaders.metal
        )

        add_metal_shader_library(default
            STANDARD macos-metal1.1
            ${ShaderBase_SOURCES}
            ${ShaderBase_HEADERS}
        )

        target_embed_metal_shader_libraries(visualboyadvance-m
            default
        )
    endif()

# Needed for CoreAudio.
    target_compile_options(visualboyadvance-m PRIVATE -Wno-error=elaborated-enum-base)
endif()

target_sources(visualboyadvance-m PRIVATE ${VBAM_WX_COMMON} ${VBAM_ICON_PATH})

if(ENABLE_SDL3)
    target_include_directories(visualboyadvance-m PRIVATE ${SDL3_INCLUDE_DIRS})
else()
    target_include_directories(visualboyadvance-m PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

if(ENABLE_OPENAL)
    find_package(OpenAL REQUIRED)

    target_sources(visualboyadvance-m PRIVATE
        audio/internal/openal.cpp
        audio/internal/openal.h
    )

    target_compile_definitions(visualboyadvance-m PRIVATE VBAM_ENABLE_OPENAL)
endif()


target_link_libraries(
    visualboyadvance-m
    vbam-components-draw-text
    vbam-components-filters
    vbam-components-filters-agb
    vbam-components-filters-cgb
    vbam-components-filters-interframe
    vbam-components-user-config
    vbam-wx-config
    vbam-wx-widgets
)

# adjust link command when making a static binary for gcc
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND VBAM_STATIC)
    if(MSYS)
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

add_dependencies(visualboyadvance-m translations-zip)

# on unix we have to check for X11 before we overwrite all the compile/link
# flags with the wx tests
if(NOT WIN32 AND NOT APPLE)
    find_package(X11)

    if(X11_X11_LIB)
        target_include_directories(visualboyadvance-m PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(visualboyadvance-m ${X11_X11_LIB})
        target_compile_definitions(visualboyadvance-m PRIVATE HAVE_X11)
        if(X11_Xscreensaver_LIB)
        target_link_libraries(visualboyadvance-m ${X11_Xscreensaver_LIB})
            target_compile_definitions(visualboyadvance-m PRIVATE HAVE_XSS)
        endif()
    endif()

    find_library(EGL_LIBRARY EGL)

    if(EGL_LIBRARY)
    target_link_libraries(visualboyadvance-m ${EGL_LIBRARY})
        target_compile_definitions(visualboyadvance-m PRIVATE HAVE_EGL)
    endif()

    find_library(WAYLAND_LIBRARY wayland-client)
    if(WAYLAND_LIBRARY)
        target_link_libraries(visualboyadvance-m ${WAYLAND_LIBRARY})
    endif()
endif()

if(WIN32)
    target_sources(visualboyadvance-m
        PRIVATE
        audio/internal/dsound.cpp
        wxvbam.rc
        wxvbam_translations.rc
    )
    target_link_libraries(visualboyadvance-m
        dxguid dsound wsock32 ws2_32 imm32 version winmm)

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

if(APPLE)
    target_sources(visualboyadvance-m PRIVATE
        macsupport.mm
    )

    target_link_libraries(visualboyadvance-m
      tiff)

    if(NOT NIX)
        target_link_libraries(visualboyadvance-m
          zstd deflate)
    endif()
endif()

# link libgcc/libstdc++ statically on mingw
# and adjust link command when making a static binary
if(CMAKE_COMPILER_IS_GNUCXX AND VBAM_STATIC)
    # some dists don't have a static libpthread
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread")

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

# XAudio2.
if(ENABLE_XAUDIO2)
    target_sources(visualboyadvance-m PRIVATE audio/internal/xaudio2.cpp)
endif()

# FAudio.
if(ENABLE_FAUDIO)
    target_sources(visualboyadvance-m PRIVATE audio/internal/faudio.cpp)
endif()

configure_wx_target(visualboyadvance-m)

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
        target_include_directories(visualboyadvance-m PRIVATE ${GTK4_INCLUDE_DIRS})
        target_link_directories(visualboyadvance-m PRIVATE ${GTK4_LIBRARY_DIRS})
        target_compile_options(visualboyadvance-m PRIVATE ${GTK4_CFLAGS_OTHER})
        target_link_libraries(visualboyadvance-m ${GTK4_LIBRARIES})
    elseif(WX_USING_GTK3)
        pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
        if(NOT GTK3_INCLUDE_DIRS)
            message(FATAL_ERROR "Could not find gtk3")
        endif()
        target_include_directories(visualboyadvance-m PRIVATE ${GTK3_INCLUDE_DIRS})
        target_link_directories(visualboyadvance-m PRIVATE ${GTK3_LIBRARY_DIRS})
        target_compile_options(visualboyadvance-m PRIVATE ${GTK3_CFLAGS_OTHER})
        target_link_libraries(visualboyadvance-m ${GTK3_LIBRARIES})
    else()
        check_cxx_symbol_exists(__WXGTK20__ ${WX_CONFIG_H} WX_USING_GTK2)
        if(WX_USING_GTK2)
            # try to use pkg-config to find gtk2 first
            pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
            if(GTK2_INCLUDE_DIRS)
                target_include_directories(visualboyadvance-m PRIVATE ${GTK2_INCLUDE_DIRS})
                target_link_directories(visualboyadvance-m PRIVATE ${GTK2_LIBRARY_DIRS})
                target_compile_options(visualboyadvance-m PRIVATE ${GTK2_CFLAGS_OTHER})
                target_link_libraries(visualboyadvance-m ${GTK2_LIBRARIES})
            else()
                # and if that fails, use the cmake module
                find_package(GTK2 REQUIRED gtk)
                if(NOT GTK2_INCLUDE_DIRS)
                    message(FATAL_ERROR "Could not find gtk2")
                endif()
                target_include_directories(visualboyadvance-m PRIVATE ${GTK2_INCLUDE_DIRS})
                target_compile_options(visualboyadvance-m PRIVATE ${GTK2_DEFINITIONS})
                target_link_libraries(visualboyadvance-m ${GTK2_LIBRARIES})
            endif()
        else()
            find_package(GTK REQUIRED gtk)
            if(NOT GTK_INCLUDE_DIRS)
                message(FATAL_ERROR "Could not find gtk")
            endif()
            target_include_directories(visualboyadvance-m PRIVATE ${GTK_INCLUDE_DIRS})
            target_compile_options(visualboyadvance-m PRIVATE ${GTK_DEFINITIONS})
            target_link_libraries(visualboyadvance-m ${GTK_LIBRARIES})
        endif()
    endif()
endif()

# FFMPeg.
if(ENABLE_FFMPEG)
    target_link_libraries(
        visualboyadvance-m
        vbam-components-av-recording
        ${FFMPEG_LIBRARIES}
    )

    if(FFMPEG_LDFLAGS)
        target_link_options(visualboyadvance-m PRIVATE ${FFMPEG_LDFLAGS})
    endif()
endif()

target_link_libraries(visualboyadvance-m nonstd-lib)

# wxrc does not support xrs files in -c output (> 10x compression)
# we do it using the bin2c.c utility.
set(BIN2C ${CMAKE_BINARY_DIR}/bin2c)
include(HostCompile)
host_compile(${CMAKE_CURRENT_SOURCE_DIR}/bin2c.c ${BIN2C})

# Override wxrc when cross-compiling.
if(CMAKE_HOST_WIN32 AND CMAKE_CROSSCOMPILING)
    set(WXRC ${CMAKE_SOURCE_DIR}/win32-deps/wxrc/wxrc.exe)
endif()

# Configure wxrc.
if(WXRC AND NOT CMAKE_HOST_WIN32)
    separate_arguments(WXRC UNIX_COMMAND ${WXRC})
elseif(DEFINED ENV{WXRC})
    if(NOT CMAKE_HOST_WIN32)
        separate_arguments(WXRC UNIX_COMMAND "$ENV{WXRC}")
    else()
        set(WXRC "$ENV{WXRC}")
    endif()
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
    if(NOT WXRC)
        message(FATAL_ERROR "Could not find a wxrc executable.")
    endif()
endif()

# Build the resources.
set(
    VBAM_XRC_FILES
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
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs
    COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{PATH}" ${WXRC} ${VBAM_XRC_FILES} -o ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${VBAM_XRC_FILES}
)

add_custom_command(
    OUTPUT ${VBAM_GENERATED_DIR}/wx/builtin-xrc.h
    COMMAND ${BIN2C} ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs ${VBAM_GENERATED_DIR}/wx/builtin-xrc.h builtin_xrs
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/wxvbam.xrs ${BIN2C}
)

# use a built-in vba-over.ini if no config file present
add_custom_command(
    OUTPUT ${VBAM_GENERATED_DIR}/wx/builtin-over.h
    COMMAND ${BIN2C} ${CMAKE_SOURCE_DIR}/src/vba-over.ini ${VBAM_GENERATED_DIR}/wx/builtin-over.h builtin_over
    DEPENDS ${CMAKE_SOURCE_DIR}/src/vba-over.ini ${BIN2C}
)

add_custom_command(
    OUTPUT ${VBAM_GENERATED_DIR}/wx/gb-builtin-over.h
    COMMAND ${BIN2C} ${CMAKE_SOURCE_DIR}/src/gb-over.ini ${VBAM_GENERATED_DIR}/wx/gb-builtin-over.h gb_builtin_over
    DEPENDS ${CMAKE_SOURCE_DIR}/src/gb-over.ini ${BIN2C}
)

set(VBAM_LOCALIZABLE_FILES ${VBAM_WX_COMMON} ${VBAM_LOCALIZABLE_WX_CONFIG_FILES})
list(APPEND VBAM_LOCALIZABLE_FILES
    audio/internal/dsound.cpp
    audio/internal/faudio.cpp
    audio/internal/xaudio2.cpp
    autoupdater/autoupdater.h
    autoupdater/macos/autoupdater.cpp
    autoupdater/macos/sparkle-wrapper.h
    autoupdater/wxmsw/autoupdater.cpp
    autoupdater/wxmsw/winsparkle-rc.h
    autoupdater/wxmsw/winsparkle-wrapper.cpp
    autoupdater/wxmsw/winsparkle-wrapper.h
    widgets/dpi-support.cpp
    widgets/dpi-support-mac.mm
    ${CMAKE_SOURCE_DIR}/src/core/gba/gbaLink.cpp
)

if(APPLE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
endif()

if(WIN32 AND (X86_64 OR ARM64 OR X86) AND ENABLE_ONLINEUPDATES)
    if(NOT DEFINED WINSPARKLE_BIN_RELEASE_DIR)
        set(WINSPARKLE_BIN_RELEASE_DIR ${CMAKE_SOURCE_DIR}/win32-deps/WinSparkle-0.9.1)
    endif()

    target_include_directories(
        visualboyadvance-m
        PRIVATE ${WINSPARKLE_BIN_RELEASE_DIR}/include
    )

    if(X86_64 AND MINGW)
        set(WINSPARKLE_DLL ${WINSPARKLE_BIN_RELEASE_DIR}/mingw/x86_64/WinSparkle.dll)
    elseif(X86 AND MINGW)
        set(WINSPARKLE_DLL ${WINSPARKLE_BIN_RELEASE_DIR}/mingw/i686/WinSparkle.dll)
    elseif(X86_64)
        set(WINSPARKLE_DLL ${WINSPARKLE_BIN_RELEASE_DIR}/x64/Release/WinSparkle.dll)
    elseif(ARM64)
        set(WINSPARKLE_DLL ${WINSPARKLE_BIN_RELEASE_DIR}/ARM64/Release/WinSparkle.dll)
    elseif(X86)
        set(WINSPARKLE_DLL ${WINSPARKLE_BIN_RELEASE_DIR}/Release/WinSparkle.dll)
    endif()

    target_sources(visualboyadvance-m PRIVATE
        autoupdater/wxmsw/autoupdater.cpp
        autoupdater/wxmsw/winsparkle-rc.h
        autoupdater/wxmsw/winsparkle-wrapper.cpp
        autoupdater/wxmsw/winsparkle-wrapper.h)

    configure_file(autoupdater/wxmsw/winsparkle-path.h.in ${CMAKE_SOURCE_DIR}/src/wx/winsparkle-path.h)
endif()

if(APPLE AND ENABLE_ONLINEUPDATES)
    include(FetchContent)
    FetchContent_Declare(Sparkle
        URL "https://github.com/andyvand/Sparkle/releases/download/Release/Sparkle-2.7.1-MacOSX10.10.tar.xz"
        URL_HASH SHA256=2d2419118c2ee8fe6e740a953b74cfa3b367606e4bab3fe0ab63332a5324ac5f
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(Sparkle)

    target_sources(visualboyadvance-m PRIVATE
        autoupdater/autoupdater.h
        autoupdater/macos/autoupdater.cpp
        autoupdater/macos/sparkle-wrapper.h
        autoupdater/macos/sparkle-wrapper.mm)

    find_library(SPARKLE_FRAMEWORK
        NAMES Sparkle
        HINTS ${sparkle_SOURCE_DIR}
    )
    find_path(SPARKLE_INCLUDE_DIR Sparkle.h HINTS ${SPARKLE_FRAMEWORK}/Headers)
    target_include_directories(
        visualboyadvance-m
        PRIVATE ${SPARKLE_INCLUDE_DIR}
    )

    set(APPCAST_URL "https://data.visualboyadvance-m.org/appcast.xml")
    set(CCS1 rm -rf ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/Sparkle.framework)
    set(CCS2 mkdir -p ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks)
    # The following commands *should* be run to make sure Sparkle is not going
    # to bug randomly.
    set(CCS3 cp -a ${SPARKLE_FRAMEWORK} ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Frameworks/Sparkle.framework)
    set(CCS6 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUEnableAutomaticChecks -bool YES)
    #set(CCS7 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUPublicEDKey -string "${PUBLIC_KEY}")
    set(CCS8 defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist SUFeedURL -string "${APPCAST_URL}")
    add_custom_command(TARGET visualboyadvance-m POST_BUILD
        COMMAND ${CCS1}
        COMMAND ${CCS2}
        COMMAND ${CCS3}
        COMMAND ${CCS6}
        #COMMAND ${CCS7}
        COMMAND ${CCS8}
    )

    TARGET_LINK_LIBRARIES(visualboyadvance-m $<LINK_LIBRARY:WEAK_FRAMEWORK,${SPARKLE_FRAMEWORK}>)
endif()

if(NOT WIN32 AND NOT APPLE)
    install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/visualboyadvance-m.desktop     DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
    install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/visualboyadvance-m.metainfo.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/metainfo)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/icons/sizes/                   DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor)
endif()

# Update the gettext pot source.
# Do this automatically instead of manually to make sure we don't forget to update.
add_custom_target(
    vbam-wx-xrc-strings
    COMMAND ${CMAKE_COMMAND}
        -E env
        "PATH=$ENV{PATH}"
        ${WXRC}
        -g ${VBAM_XRC_FILES}
        -o ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
    BYPRODUCTS
        ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${VBAM_XRC_FILES}
)

add_custom_target(
    vbam-wx-pot
    COMMAND ${XGETTEXT}
        -k_ -kN_ 
        -o ${CMAKE_BINARY_DIR}/wxvbam.pot
        ${VBAM_LOCALIZABLE_FILES} ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
    BYPRODUCTS
        ${CMAKE_BINARY_DIR}/wxvbam.pot
    DEPENDS
        ${VBAM_LOCALIZABLE_FILES} ${CMAKE_BINARY_DIR}/wx-xrc-strings.h
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(
    TARGET vbam-wx-pot
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC_DIR=${CMAKE_SOURCE_DIR}/po/wxvbam
        -DBIN_DIR=${CMAKE_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check-pot-updated.cmake
)

find_program(GPG_PROGRAM gpg)
if(GPG_PROGRAM)
    execute_process(
        COMMAND ${GPG_PROGRAM} -k
        OUTPUT_VARIABLE GPG_KEYS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(NOT ZIP_SUFFIX)
    set(ZIP_SUFFIX "")
endif()

if(UPSTREAM_RELEASE AND WIN32)
    set(home "$ENV{HOME}")

    if(NOT home OR ((MSVC OR NOT CMAKE_CROSSCOMPILING) AND NOT DEFINED ENV{MSYSTEM_PREFIX}))
        set(home "$ENV{USERPROFILE}")
    endif()

    # rewrite backslashes to slashes, needed for msys osslsigncode
    string(REGEX REPLACE "\\\\" "/" home "${home}")

    set(cert "${home}/.codesign/windows_comodo.pkcs12")
    file(STRINGS "${home}/.codesign/windows_comodo.pkcs12.password" cert_password)

    if(EXISTS "${cert}" AND cert_password)
        find_program(OSSLSIGNCODE_PROGRAM osslsigncode)
        find_program(SIGNTOOL_PROGRAM     signtool)

        if(OSSLSIGNCODE_PROGRAM)
            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rename visualboyadvance-m.exe visualboyadvance-m-unsigned.exe
                COMMAND ${OSSLSIGNCODE_PROGRAM} sign -pkcs12 ${cert} -pass "${cert_password}" -t http://timestamp.digicert.com -n visualboyadvance-m -i https://github.com/visualboyadvance-m/visualboyadvance-m -in visualboyadvance-m-unsigned.exe -out visualboyadvance-m.exe
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
        elseif(SIGNTOOL_PROGRAM)
            add_custom_command(
                TARGET visualboyadvance-m
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy visualboyadvance-m.exe visualboyadvance-m-unsigned.exe
                COMMAND ${SIGNTOOL_PROGRAM} sign /f ${cert} /fd certHash /td certHash /p "${cert_password}" /tr http://timestamp.digicert.com /du https://github.com/visualboyadvance-m/visualboyadvance-m /a visualboyadvance-m.exe
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

    if(MSVC AND CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
        set(pdb_file visualboyadvance-m.pdb)
    endif()

    add_custom_command(
        TARGET visualboyadvance-m
        POST_BUILD
        COMMAND ${ZIP_PROGRAM} -9 ${exezip} visualboyadvance-m.exe ${pdb_file}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    if(GPG_SIGNATURES AND GPG_KEYS)
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
    set(MACOSX_BUNDLE_ICON_FILE ${VBAM_ICON})
    set_property(TARGET visualboyadvance-m APPEND PROPERTY MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wxplist.in)
    set_source_files_properties(${VBAM_ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # bundle dylibs and relink them for releasing .app
    # also install translations into the .app
    # but only in Release mode
    option(BUNDLE_DYLIBS "Bundle dylibs into .app" OFF)

    if(GUNDLE_DYLIBS AND CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
        add_custom_command(
            TARGET visualboyadvance-m POST_BUILD
            COMMAND bash ${CMAKE_SOURCE_DIR}/tools/macOS/third_party_libs_tool ./visualboyadvance-m.app
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        )
    endif()

    add_custom_command(
        TARGET visualboyadvance-m POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/mac-translations.cmake
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    set(ver ${VBAM_VERSION})

    if(NOT VBAM_VERSION_RELEASE)
        set(ver "${ver}-${VBAM_REVISION}")
    endif()

    add_custom_command(
        TARGET visualboyadvance-m POST_BUILD
        COMMAND defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist CFBundleVersion            -string "${ver}"
        COMMAND defaults write ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist CFBundleShortVersionString -string "${ver}"
        COMMAND plutil -convert xml1 ${CMAKE_BINARY_DIR}/visualboyadvance-m.app/Contents/Info.plist
    )
endif()

if(APPLE AND UPSTREAM_RELEASE)
#    if(WX_MAC_PATCHED)
#        target_compile_definitions(visualboyadvance-m PRIVATE VBAM_WX_MAC_PATCHED_FOR_ALERT_SOUND)
#    endif()

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
        VERBATIM COMMAND sh -c [=[codesign --sign "Developer ID Application" --options runtime --timestamp --force --deep ./visualboyadvance-m.app || :]=]
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
                VERBATIM COMMAND sh -c "codesign --sign 'Developer ID Application' --options runtime --timestamp --force --deep ${framework} || :"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endforeach()
    endif()

    # Zip, notarize, staple to the .app and zip again

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

    add_custom_command(
        TARGET visualboyadvance-m
        POST_BUILD
        COMMAND xcrun notarytool submit ${appzip} --keychain-profile AC_PASSWORD --wait
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    add_custom_command(
        TARGET visualboyadvance-m
        POST_BUILD
        COMMAND xcrun stapler staple ./visualboyadvance-m.app
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

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
            COMMAND ${GPG_PROGRAM} --detach-sign -a ${appzip}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
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

# Installation scripts.
install(
    PROGRAMS ${PROJECT_BINARY_DIR}/visualboyadvance-m${CMAKE_EXECUTABLE_SUFFIX}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    FILES ${CMAKE_SOURCE_DIR}/src/vba-over.ini
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vbam
)

if (UNIX)
    install(FILES ${CMAKE_SOURCE_DIR}/src/debian/visualboyadvance-m.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
endif()
