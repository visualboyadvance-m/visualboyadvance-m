option(BUILD_TESTING "Build testing" ON)
option(BUILD_SHARED_LIBS "Build dynamic libraries" OFF)



option(ENABLE_SDL "Build the SDL port" ON)
option(ENABLE_WX "Build the wxWidgets port" ON)
option(ENABLE_DEBUGGER "Enable the debugger" ON)
option(ENABLE_ASAN "Enable -fsanitize=<option>, address by default, requires debug build" OFF)

option(ENABLE_SSP "Enable gcc stack protector support" OFF)
option(VBAM_STATIC "Try to link all libraries statically" ${VBAM_STATIC_DEFAULT})

option(ENABLE_ASM "Enable x86 ASM related options" OFF)

# The ARM ASM core seems to be very buggy, see #98 and #54. Default to it being
# OFF for the time being, until it is either fixed or replaced.
option(ENABLE_ASM_CORE "Enable x86 ASM CPU cores" OFF)

set(ASM_SCALERS_DEFAULT ${ENABLE_ASM})
set(MMX_DEFAULT ${ENABLE_ASM})

option(ENABLE_ASM_SCALERS "Enable x86 ASM graphic filters" ${ASM_SCALERS_DEFAULT})

include(CMakeDependentOption)
cmake_dependent_option(ENABLE_MMX "Enable MMX" ${MMX_DEFAULT} "ENABLE_ASM_SCALERS" OFF)

option(ENABLE_LINK "Enable GBA linking functionality" ON)

option(ENABLE_LIRC "Enable LIRC support" OFF)

option(ENABLE_FFMPEG "Enable ffmpeg A/V recording" ON)

set(ONLINEUPDATES_DEFAULT OFF)

if((WIN32 AND (X64 OR X86)) OR APPLE) # winsparkle/sparkle
    set(ONLINEUPDATES_DEFAULT ON)
endif()

option(ENABLE_ONLINEUPDATES "Enable online update checks" ${ONLINEUPDATES_DEFAULT})
option(HTTPS "Use https URL for winsparkle" ON)

set(LTO_DEFAULT ON)
# gcc lto produces buggy binaries for 64 bit mingw
# and we generally don't want it when debugging because it makes linking slow
if(CMAKE_BUILD_TYPE STREQUAL Debug OR (WIN32 AND CMAKE_COMPILER_IS_GNUCXX))
    set(LTO_DEFAULT OFF)
endif()

option(ENABLE_LTO "Compile with Link Time Optimization (gcc and clang only)" ${LTO_DEFAULT})

option(ENABLE_GBA_LOGGING "Enable extended GBA logging" ON)

# Add support for Homebrew, MacPorts and Fink on macOS
option(DISABLE_MACOS_PACKAGE_MANAGERS "Set to TRUE to disable support for macOS Homebrew, MacPorts and Fink." FALSE)

if(APPLE AND NOT DISABLE_MACOS_PACKAGE_MANAGERS)
    include(MacPackageManagers)
endif()

option(ENABLE_NLS "Enable translations" ON)

option(UPSTREAM_RELEASE "do some optimizations and release automation tasks" OFF)

option(TRANSLATIONS_ONLY "Build only the translations.zip" OFF)

if(WIN32)
    # not yet implemented
    option(ENABLE_DIRECT3D "Enable Direct3D rendering for the wxWidgets port" OFF)

    option(ENABLE_XAUDIO2 "Enable xaudio2 sound output for the wxWidgets port" ON)
endif()

option(ENABLE_FAUDIO "Enable FAudio sound output for the wxWidgets port" OFF)

option(ZIP_SUFFIX [=[suffix for release zip files, e.g.  "-somebranch".zip]=] OFF)

option(ENABLE_OPENAL "Enable OpenAL for the wxWidgets port" ON)
