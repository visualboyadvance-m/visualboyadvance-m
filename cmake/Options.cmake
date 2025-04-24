option(BUILD_TESTING "Build testing" ON)
option(BUILD_SHARED_LIBS "Build dynamic libraries" OFF)

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

option(ENABLE_SDL "Build the SDL port" ${ENABLE_SDL_DEFAULT})
option(ENABLE_WX "Build the wxWidgets port" ${BUILD_DEFAULT})
option(ENABLE_DEBUGGER "Enable the debugger" ON)
option(ENABLE_ASAN "Enable -fsanitize=address by default. Requires debug build with GCC/Clang" OFF)

# Static linking
set(VBAM_STATIC_DEFAULT OFF)
if(VCPKG_TARGET_TRIPLET MATCHES -static OR CMAKE_TOOLCHAIN_FILE MATCHES "mxe|-static")
    set(VBAM_STATIC_DEFAULT ON)
elseif(MINGW OR MSYS)
    # Default to static builds on MinGW and all MSYS2 envs.
    set(VBAM_STATIC_DEFAULT ON)
endif()
option(VBAM_STATIC "Try to link all libraries statically" ${VBAM_STATIC_DEFAULT})

if(VBAM_STATIC)
    set(SDL2_STATIC ON)
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

find_package(PkgConfig)

# Link / SFML
if(NOT TRANSLATIONS_ONLY)
    set(ENABLE_LINK_DEFAULT ON)
endif()
option(ENABLE_LINK "Enable GBA linking functionality" ${ENABLE_LINK_DEFAULT})

# FFMpeg
set(FFMPEG_DEFAULT OFF)
set(FFMPEG_COMPONENTS         AVCODEC            AVFORMAT            SWSCALE          AVUTIL            SWRESAMPLE)
set(FFMPEG_COMPONENT_VERSIONS AVCODEC>=58.18.100 AVFORMAT>=58.12.100 SWSCALE>=5.1.100 AVUTIL>=56.14.100 SWRESAMPLE>=3.1.100)

if(NOT TRANSLATIONS_ONLY AND (NOT DEFINED ENABLE_FFMPEG OR ENABLE_FFMPEG) AND (NOT (X86 AND MINGW)))
    set(FFMPEG_DEFAULT ON)

    find_package(FFmpeg COMPONENTS ${FFMPEG_COMPONENTS})
    # check versions, but only if pkgconfig is available
    if(FFMPEG_FOUND AND PKG_CONFIG_FOUND AND NOT CMAKE_TOOLCHAIN_FILE MATCHES vcpkg)
        foreach(component ${FFMPEG_COMPONENT_VERSIONS})
            string(REPLACE ">=" ";" parts ${component})
            list(GET parts 0 name)
            list(GET parts 1 version)

            if((NOT DEFINED ${name}_VERSION) OR ${name}_VERSION VERSION_LESS ${version})
                set(FFMPEG_FOUND OFF)
            endif()
        endforeach()
    endif()

    if(NOT FFMPEG_FOUND)
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
    # not yet implemented
    option(ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" OFF)

    set(XAUDIO2_DEFAULT ON)
    if (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        # TODO: We should update the XAudio headers to build with clang-cl. See
        # https://github.com/visualboyadvance-m/visualboyadvance-m/issues/1021
        set(XAUDIO2_DEFAULT OFF)
    endif()
    option(ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ${XAUDIO2_DEFAULT})
endif()

set(ENABLE_FAUDIO_DEFAULT OFF)

find_package(FAudio QUIET)

if(FAudio_FOUND AND NOT (MINGW AND X86))
    set(ENABLE_FAUDIO_DEFAULT ON)
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
