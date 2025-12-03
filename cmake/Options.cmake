option(BUILD_TESTING "Build testing" ON)
option(BUILD_SHARED_LIBS "Build dynamic libraries" OFF)
option(COMPILE_ONLY "Compile source files only, skip linking executables" OFF)

# Detect CI environment or allow explicit setting
if(DEFINED ENV{CI} OR DEFINED ENV{GITHUB_ACTIONS} OR DEFINED ENV{GITLAB_CI})
    set(VBAM_CI_DEFAULT ON)
else()
    set(VBAM_CI_DEFAULT OFF)
endif()
option(ENABLE_WERROR "Treat warnings as errors (enabled in CI)" ${VBAM_CI_DEFAULT})

option(TRANSLATIONS_ONLY "Build only the translations.zip" OFF)
if(TRANSLATIONS_ONLY)
    set(BUILD_DEFAULT OFF)
else()
    set(BUILD_DEFAULT ON)
endif()

set(ENABLE_SDL_DEFAULT ${BUILD_DEFAULT})

if(WIN32 OR APPLE)
    set(ENABLE_SDL_DEFAULT OFF)
endif()

# Static linking
set(VBAM_STATIC_DEFAULT OFF)
if(VCPKG_TARGET_TRIPLET MATCHES -static OR CMAKE_TOOLCHAIN_FILE MATCHES "mxe|-static")
    set(VBAM_STATIC_DEFAULT ON)
elseif(MINGW OR MSYS)
    # Default to static builds on MinGW and all MSYS2 envs.
    set(VBAM_STATIC_DEFAULT ON)
endif()

option(VBAM_STATIC "Try to link all libraries statically" ${VBAM_STATIC_DEFAULT})

# This is a commonly used CMake option.
if(DEFINED ENABLE_SHARED)
   if(NOT ENABLE_SHARED)
      set(VBAM_STATIC ON)
   else()
      set(VBAM_STATIC OFF)
   endif()
endif()

if(VBAM_STATIC)
    set(SDL2_STATIC ON)
    set(SDL3_STATIC ON)
    set(SFML_STATIC_LIBRARIES ON)
    set(FFMPEG_STATIC ON)
    set(OPENAL_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_START_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_END_STATIC   ON)

    if(MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
    else()
        list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .a)
    endif()
endif()

if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
   set(PKG_CONFIG_EXECUTABLE "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/pkgconf/pkgconf.exe")
endif()

find_package(PkgConfig)

if(UNIX AND NOT APPLE)
    pkg_check_modules(SDL3 sdl3 QUIET)
else()
    find_package(SDL3 QUIET)
endif()

option(ENABLE_SDL3 "Use SDL3" "${SDL3_FOUND}")

if(NOT TRANSLATIONS_ONLY)
    if(ENABLE_SDL3)
        if(NOT UNIX)
            find_package(SDL3 REQUIRED)
        endif()
    else()
        find_package(SDL2 REQUIRED)
    endif()
endif()

option(ENABLE_GENERIC_FILE_DIALOGS "Use generic file dialogs" OFF)
option(DISABLE_OPENGL "Disable OpenGL" OFF)
option(ENABLE_SDL "Build the SDL port" ${ENABLE_SDL_DEFAULT})
option(ENABLE_WX "Build the wxWidgets port" ${BUILD_DEFAULT})
option(ENABLE_DEBUGGER "Enable the debugger" ON)
option(ENABLE_ASAN "Enable -fsanitize=address by default. Requires debug build with GCC/Clang" OFF)
option(ENABLE_BZ2 "Enable BZ2 archive support" ON)
option(ENABLE_LZMA "Enable LZMA archive support" ON)

if(ENABLE_SDL3)
   set(CMAKE_C_FLAGS "-DENABLE_SDL3 ${CMAKE_C_FLAGS}")
   set(CMAKE_CXX_FLAGS "-DENABLE_SDL3 ${CMAKE_CXX_FLAGS}")

   include(CheckSourceCompiles)
   check_source_compiles(CXX
"#include <SDL3/SDL.h>

int main() { return SDL_SCALEMODE_PIXELART; }
"       HAVE_SDL_SCALEMODE_PIXELART)

   if(HAVE_SDL_SCALEMODE_PIXELART)
      set(CMAKE_C_FLAGS "-DHAVE_SDL3_PIXELART ${CMAKE_C_FLAGS}")
      set(CMAKE_CXX_FLAGS "-DHAVE_SDL3_PIXELART ${CMAKE_CXX_FLAGS}")
   endif()
endif()

if(DISABLE_OPENGL)
   set(CMAKE_C_FLAGS "-DNO_OPENGL -DNO_OGL ${CMAKE_C_FLAGS}")
   set(CMAKE_CXX_FLAGS "-DNO_OPENGL -DNO_OGL ${CMAKE_CXX_FLAGS}")
endif()

option(ENABLE_ASM "Enable x86 ASM related options" OFF)

# The ARM ASM core seems to be very buggy, see #98 and #54. Default to it being
# OFF for the time being, until it is either fixed or replaced.
option(ENABLE_ASM_CORE "Enable x86 ASM CPU cores (EXPERIMENTAL)" OFF)

set(ASM_SCALERS_DEFAULT ${ENABLE_ASM})
set(MMX_DEFAULT ${ENABLE_ASM})

option(ENABLE_ASM_SCALERS "Enable x86 ASM graphic filters" ${ASM_SCALERS_DEFAULT})

include(CMakeDependentOption)
cmake_dependent_option(ENABLE_MMX "Enable MMX" ${MMX_DEFAULT} "ENABLE_ASM_SCALERS" OFF)

option(ENABLE_LIRC "Enable LIRC support" OFF)

# Add support for Homebrew, MacPorts and Fink on macOS
option(DISABLE_MACOS_PACKAGE_MANAGERS "Set to TRUE to disable support for macOS Homebrew, MacPorts and Fink." FALSE)
if(APPLE AND NOT DISABLE_MACOS_PACKAGE_MANAGERS)
    include(MacPackageManagers)
endif()

# Link / SFML
if(NOT TRANSLATIONS_ONLY)
    set(ENABLE_LINK_DEFAULT ON)
endif()
option(ENABLE_LINK "Enable GBA linking functionality" ${ENABLE_LINK_DEFAULT})

# FFMpeg
set(FFMPEG_DEFAULT OFF)
set(FFMPEG_COMPONENTS         AVFORMAT            AVCODEC            SWSCALE          AVUTIL            SWRESAMPLE          X264    X265)
set(FFMPEG_COMPONENT_VERSIONS AVFORMAT>=58.12.100 AVCODEC>=58.18.100 SWSCALE>=5.1.100 AVUTIL>=56.14.100 SWRESAMPLE>=3.1.100 X264>=0 X265>=0)

if(NOT TRANSLATIONS_ONLY AND (NOT DEFINED ENABLE_FFMPEG OR ENABLE_FFMPEG))
    set(FFMPEG_DEFAULT ON)

    find_package(FFmpeg COMPONENTS ${FFMPEG_COMPONENTS})

    # check versions, but only if pkgconfig is available
    if(FFmpeg_FOUND AND PKG_CONFIG_FOUND AND NOT CMAKE_TOOLCHAIN_FILE MATCHES vcpkg)
        foreach(component ${FFMPEG_COMPONENT_VERSIONS})
            string(REPLACE ">=" ";" parts ${component})
            list(GET parts 0 name)
            list(GET parts 1 version)

            if((NOT DEFINED ${name}_VERSION) OR ${name}_VERSION VERSION_LESS ${version})
                set(FFmpeg_FOUND OFF)
            endif()
        endforeach()
    endif()

    if(NOT FFmpeg_FOUND)
        set(FFMPEG_DEFAULT OFF)
    endif()
endif()
option(ENABLE_FFMPEG "Enable ffmpeg A/V recording" ${FFMPEG_DEFAULT})

# Online Updates
set(ONLINEUPDATES_DEFAULT OFF)
if(DEFINED(UPSTREAM_RELEASE) AND UPSTREAM_RELEASE)
    set(ONLINEUPDATES_DEFAULT ON)
endif()
option(ENABLE_ONLINEUPDATES "Enable online update checks" ${ONLINEUPDATES_DEFAULT})
option(HTTPS "Use https URL for winsparkle" ON)

# We generally don't want LTO when debugging because it makes linking slow
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LTO_DEFAULT OFF)
else()
    set(LTO_DEFAULT ON)
endif()
option(ENABLE_LTO "Compile with Link Time Optimization" ${LTO_DEFAULT})

option(ENABLE_GBA_LOGGING "Enable extended GBA logging" ON)

option(UPSTREAM_RELEASE "do some optimizations and release automation tasks" OFF)

if(WIN32)
    option(ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" ON)

    set(XAUDIO2_DEFAULT ON)
    if ((MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL Clang))
        # TODO: We should update the XAudio headers to build with clang-cl. See
        # https://github.com/visualboyadvance-m/visualboyadvance-m/issues/1021
        set(XAUDIO2_DEFAULT OFF)
    endif()
    option(ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ${XAUDIO2_DEFAULT})
endif()

find_package(OpenAL QUIET)

set(OPENAL_DEFAULT ${OpenAL_FOUND})

if(MINGW AND X86)
    # OpenAL-Soft uses avrt.dll which is not available on Windows XP.
    set(OPENAL_DEFAULT OFF)
endif()

option(ENABLE_OPENAL "Enable OpenAL-Soft sound output for the wxWidgets port" ${OPENAL_DEFAULT})

set(ENABLE_FAUDIO_DEFAULT OFF)

find_package(FAudio QUIET)

if(FAudio_FOUND)
    # Check that FAudio links to the same SDL version we're using.
    # FAudio built with SDL2 won't work with SDL3 and vice versa.
    set(_faudio_sdl_mismatch OFF)

    # Check INTERFACE_LINK_LIBRARIES on FAudio targets for SDL version.
    foreach(_faudio_target FAudio::FAudio FAudio::FAudio-shared FAudio::FAudio-static)
        if(TARGET ${_faudio_target})
            get_target_property(_faudio_link_libs ${_faudio_target} INTERFACE_LINK_LIBRARIES)
            if(_faudio_link_libs)
                if(ENABLE_SDL3)
                    # We're using SDL3, FAudio must link to SDL3
                    if(_faudio_link_libs MATCHES "SDL2")
                        set(_faudio_sdl_mismatch ON)
                        message(STATUS "FAudio was built with SDL2, but we're using SDL3 - disabling FAudio")
                    endif()
                else()
                    # We're using SDL2, FAudio must link to SDL2
                    if(_faudio_link_libs MATCHES "SDL3")
                        set(_faudio_sdl_mismatch ON)
                        message(STATUS "FAudio was built with SDL3, but we're using SDL2 - disabling FAudio")
                    endif()
                endif()
                break()
            endif()
        endif()
    endforeach()

    # For static libraries without INTERFACE_LINK_LIBRARIES, check symbols.
    if(NOT WIN32 AND NOT _faudio_sdl_mismatch AND VBAM_STATIC)
        foreach(_faudio_target FAudio::FAudio-static FAudio::FAudio)
            if(TARGET ${_faudio_target})
                get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION)
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELEASE)
                endif()
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELWITHDEBINFO)
                endif()
                if(_faudio_location AND EXISTS "${_faudio_location}")
                    # SDL_GetNumAudioDevices exists in SDL2 but not SDL3
                    # SDL_GetAudioPlaybackDevices exists in SDL3 but not SDL2
                    execute_process(
                        COMMAND nm -g "${_faudio_location}"
                        OUTPUT_VARIABLE _faudio_symbols
                        ERROR_QUIET
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                    )
                    if(_faudio_symbols)
                        string(FIND "${_faudio_symbols}" "SDL_GetNumAudioDevices" _has_sdl2_symbol)
                        string(FIND "${_faudio_symbols}" "SDL_GetAudioPlaybackDevices" _has_sdl3_symbol)

                        if(ENABLE_SDL3 AND _has_sdl2_symbol GREATER -1 AND _has_sdl3_symbol EQUAL -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "Static FAudio uses SDL2 symbols, but we're using SDL3 - disabling FAudio")
                        elseif(NOT ENABLE_SDL3 AND _has_sdl3_symbol GREATER -1 AND _has_sdl2_symbol EQUAL -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "Static FAudio uses SDL3 symbols, but we're using SDL2 - disabling FAudio")
                        endif()
                    endif()
                    break()
                endif()
            endif()
        endforeach()
    endif()

    # For dynamic libraries, also check with ldd/otool if target properties didn't help.
    if(NOT WIN32 AND NOT _faudio_sdl_mismatch AND NOT VBAM_STATIC)
        foreach(_faudio_target FAudio::FAudio-shared FAudio::FAudio)
            if(TARGET ${_faudio_target})
                get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION)
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELEASE)
                endif()
                if(NOT _faudio_location)
                    get_target_property(_faudio_location ${_faudio_target} IMPORTED_LOCATION_RELWITHDEBINFO)
                endif()
                if(_faudio_location AND EXISTS "${_faudio_location}")
                    if(APPLE)
                        execute_process(
                            COMMAND otool -L "${_faudio_location}"
                            OUTPUT_VARIABLE _faudio_deps
                            ERROR_QUIET
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                    elseif(UNIX)
                        execute_process(
                            COMMAND ldd "${_faudio_location}"
                            OUTPUT_VARIABLE _faudio_deps
                            ERROR_QUIET
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                        )
                    endif()
                    if(_faudio_deps)
                        string(FIND "${_faudio_deps}" "libSDL2" _links_sdl2)
                        string(FIND "${_faudio_deps}" "libSDL3" _links_sdl3)

                        if(ENABLE_SDL3 AND _links_sdl2 GREATER -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "FAudio links to libSDL2, but we're using SDL3 - disabling FAudio")
                        elseif(NOT ENABLE_SDL3 AND _links_sdl3 GREATER -1)
                            set(_faudio_sdl_mismatch ON)
                            message(STATUS "FAudio links to libSDL3, but we're using SDL2 - disabling FAudio")
                        endif()
                    endif()
                    break()
                endif()
            endif()
        endforeach()
    endif()

    if(NOT _faudio_sdl_mismatch)
        set(ENABLE_FAUDIO_DEFAULT ON)
    endif()

    unset(_faudio_sdl_mismatch)
    unset(_faudio_link_libs)
    unset(_faudio_target)
    unset(_faudio_location)
    unset(_faudio_symbols)
    unset(_faudio_deps)
    unset(_has_sdl2_symbol)
    unset(_has_sdl3_symbol)
    unset(_links_sdl2)
    unset(_links_sdl3)
endif()

option(ENABLE_FAUDIO "Enable FAudio sound output for the wxWidgets port" ${ENABLE_FAUDIO_DEFAULT})

option(ZIP_SUFFIX [=[suffix for release zip files, e.g.  "-somebranch".zip]=] OFF)

# The SDL port can't be built without debugging support
if(NOT ENABLE_DEBUGGER AND ENABLE_SDL)
    message(FATAL_ERROR "The SDL port can't be built without debugging support")
endif()

if(TRANSLATIONS_ONLY AND (ENABLE_SDL OR ENABLE_WX))
    message(FATAL_ERROR "The SDL and wxWidgets ports can't be built when TRANSLATIONS_ONLY is enabled")
endif()

option(GPG_SIGNATURES "Create GPG signatures for release files" OFF)

if(APPLE)
   set(wx_mac_patched_default OFF)

   if(UPSTREAM_RELEASE)
      set(wx_mac_patched_default ON)
   endif()

   option(WX_MAC_PATCHED "A build of wxWidgets that is patched for the alert sound bug is being used" ${wx_mac_patched_default})
endif()
