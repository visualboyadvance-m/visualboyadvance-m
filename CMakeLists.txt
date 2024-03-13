cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19...3.28.3)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(WIN32)
    include(RemoveStrawberryPerlFromPATH)
endif()

if(UPDATE_APPCAST)
    include(UpdateAppcast)
endif()

if(TAG_RELEASE)
    include(MakeReleaseCommitAndTag)
endif()

set(VCPKG_DEPS pkgconf zlib pthreads "sdl2[samplerate]" gettext wxwidgets)

set(VCPKG_DEPS_OPTIONAL
    sfml   ENABLE_LINK
    ffmpeg ENABLE_FFMPEG
)

if(WIN32)
    find_program(POWERSHELL
        NAMES powershell.exe pwsh.exe
        HINTS "/Windows/System32/WindowsPowerShell/v1.0"
        REQUIRED)
else()
    find_program(POWERSHELL pwsh REQUIRED)
endif()

include(Set-Toolchain-vcpkg)

# Use ccache if available and not already enabled on the command line.
# This has to be done before the project() call.
if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(CCACHE_EXECUTABLE ccache)
    if(CCACHE_EXECUTABLE)
        message(STATUS "Enabling ccache")

        set(CMAKE_C_COMPILER_LAUNCHER        ${CCACHE_EXECUTABLE} CACHE STRING "C compiler launcher"     FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER      ${CCACHE_EXECUTABLE} CACHE STRING "C++ compiler launcher"   FORCE)
        set(CMAKE_ASM_NASM_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE} CACHE STRING "nasm assembler launcher" FORCE)
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

project(VBA-M C CXX)

find_package(Git)
find_package(PkgConfig)

include(GNUInstallDirs)
include(Options)
include(Architecture)

if(NOT CMAKE_PREFIX_PATH AND (NOT ("$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")))
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|Debug|RelWithDebInfo|MinSizeRel)$")
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}', must be one of: 'Release', 'Debug', 'RelWithDebInfo' or 'MinSizeRel'")
endif()

# Link debug libs for RelWithDebInfo
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Debug")
endif()

include(CTest)
if(BUILD_TESTING)
    enable_testing()
endif()

set(ALL_TARGETS fex visualboyadvance-m vbamcore vbam)

add_custom_target(generate)

#Output all binaries at top level
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

if(NOT HTTPS)
    add_definitions(-DNO_HTTPS)
endif()

if(WIN32 OR CMAKE_TOOLCHAIN_FILE MATCHES "[Mm][Ii][Nn][Gg][Ww]")
    # Win32 deps submodule
    set(git_checkout FALSE)

    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
        set(git_checkout TRUE)
        execute_process(
            COMMAND git submodule update --init --remote --recursive
            RESULT_VARIABLE git_status
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        if(NOT (git_checkout AND git_status EQUAL 0))
            message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --init --remote --recursive")
        endif()
    endif()
endif()

if(MSVC)
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/msvc")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    # TODO: We should update the XAudio headers to build with clang-cl. See
    # https://github.com/visualboyadvance-m/visualboyadvance-m/issues/1021
    add_definitions(-DNO_XAUDIO2)
endif()

option(ENABLE_GBA_LOGGING "Enable extended GBA logging" ON)
if(ENABLE_GBA_LOGGING)
    add_definitions(-DGBA_LOGGING )
endif()
if(ENABLE_MMX)
    add_definitions(-DMMX)
endif()

# The SDL port can't be built without debugging support
if(NOT ENABLE_DEBUGGER AND ENABLE_SDL)
    message(SEND_ERROR "The SDL port can't be built without debugging support")
endif()

# this has to run after the toolchain is initialized so it can't be in
# Win32deps.cmake
if(MINGW)
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-include")
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    include(GitTagVersion)
    git_version(VERSION REVISION VERSION_RELEASE)

    # only use the plugin to tie the configure state to the sha to force rebuilds
    # of files that depend on version.h
    include(GetGitRevisionDescription)
    get_git_head_revision(REFSPEC COMMITHASH)

    # Make sure old tags are gone from all clones.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} tag -l
        OUTPUT_VARIABLE git_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    string(REGEX REPLACE ";"     "\\\\;" git_tags_lines "${git_tags}")
    string(REGEX REPLACE "\r?\n" ";"     git_tags_lines "${git_tags_lines}")

    set(found_old_tags FALSE)

    foreach(tag ${git_tags_lines})
        if(NOT tag MATCHES "^v[0-9]")
            set(found_old_tags TRUE)
            break()
        endif()
    endforeach()

    if(found_old_tags)
        # Delete all tags and fetch them from the origin.

        foreach(tag ${git_tags_lines})
            execute_process(
                COMMAND ${GIT_EXECUTABLE} tag -d ${tag}
                OUTPUT_QUIET
                ERROR_QUIET
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        endforeach()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} fetch --tags origin
            OUTPUT_QUIET
            ERROR_QUIET
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()
endif()

# no git or no tags, use ChangeLog
if(NOT VERSION)
    include(ChangeLogVersion)
    changelog_version(VERSION REVISION VERSION_RELEASE)
endif()

# generate version.h
include_directories(${CMAKE_BINARY_DIR})
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1,\\2,\\3,0" WIN_VERSION "${VERSION}")
configure_file("${CMAKE_SOURCE_DIR}/src/version.h.in" "${CMAKE_BINARY_DIR}/version.h" @ONLY)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
else()
    add_definitions(-DNDEBUG)
endif()

# hack for ninja in msys2
if(WIN32 AND CMAKE_GENERATOR STREQUAL Ninja AND NOT "$ENV{MSYSTEM_PREFIX}" STREQUAL "")
    set(MSYS ON)
endif()

include(VbamFunctions)

if(MSYS AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    if($ENV{MSYSTEM} STREQUAL CLANG64)
        cygpath(prefix "$ENV{MSYSTEM_PREFIX}/x86_64-w64-mingw32")
        list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    elseif($ENV{MSYSTEM} STREQUAL CLANG32)
        cygpath(prefix "$ENV{MSYSTEM_PREFIX}/i686-w64-mingw32")
        list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    endif()

    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE INTERNAL "prefix search path for find_XXXX" FORCE)
endif()

# We do not support amd64 asm yet
if(X86_64 AND (ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS OR ENABLE_MMX))
    message(FATAL_ERROR "The options ASM_CORE, ASM_SCALERS and MMX are not supported on X86_64 yet.")
endif()

if(ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS)
    if(MSVC)
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/nuget.exe)
            file(DOWNLOAD "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" ${CMAKE_BINARY_DIR}/nuget.exe)
        endif()

        execute_process(
            COMMAND nuget.exe install nasm2 -OutputDirectory ${CMAKE_BINARY_DIR}/nuget
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        file(GLOB pkg ${CMAKE_BINARY_DIR}/nuget/nasm2*)

        list(APPEND CMAKE_PROGRAM_PATH ${pkg}/tools)
    endif()

    enable_language(ASM_NASM)

    set(ASM_ENABLED ON)
endif()

if(NOT WIN32)
    find_library(PTHREAD_LIB pthread)
    if(PTHREAD_LIB)
       set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${PTHREAD_LIB})
       set(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${PTHREAD_LIB})
    endif()
elseif(MINGW)
    if(NOT VBAM_STATIC)
        set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} -lpthread)
        set(VBAMCORE_LIBS ${VBAMCORE_LIBS} -lpthread)
    else()
        set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} "-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
        set(VBAMCORE_LIBS ${VBAMCORE_LIBS} "-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
    endif()
else()
    find_package(PThreads4W)
    if(PThreads4W_FOUND)
        set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} PThreads4W::PThreads4W)
        set(VBAMCORE_LIBS ${VBAMCORE_LIBS} PThreads4W::PThreads4W)
    endif()
endif()

# Look for some dependencies using CMake scripts
find_package(ZLIB REQUIRED)

set(OpenGL_GL_PREFERENCE GLVND)

if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    set(OpenGL_GL_PREFERENCE LEGACY)
endif()

find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)

# Add libsamplerate to SDL2 with vcpkg
unset(SDL2_LIBRARY_TEMP)
if(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    if(WIN32)
        unset(arch_suffix)
        unset(path_prefix)
        if(VCPKG_TARGET_TRIPLET MATCHES -static)
            set(arch_suffix  -static)
        endif()
        if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
            set(path_prefix debug)
        endif()
        set(installed_prefix ${_VCPKG_INSTALLED_DIR}/${WINARCH}-windows${arch_suffix}/${path_prefix})

        SET(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} ${installed_prefix}/lib/samplerate.lib)
    else()
        SET(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} -lsamplerate)
    endif()
endif()

set(SDL2_TARGETS SDL2::SDL2 ${SDL2_LIBRARY_TEMP})

# set the standard libraries all ports use
set(
    VBAMCORE_LIBS
    vbamcore
    fex
    ${SDL2_TARGETS}
    ${SFML_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${ZLIB_LIBRARY}
)

if(ENABLE_FFMPEG)
    if(NOT FFMPEG_LIBRARIES)
        message(FATAL_ERROR "ENABLE_FFMPEG was specified, but required versions of ffmpeg libraries cannot be found!")
    endif()

    if(APPLE)
        set(FFMPEG_LDFLAGS ${FFMPEG_LDFLAGS} -framework CoreText -framework ApplicationServices)

        if(UPSTREAM_RELEASE)
            set(FFMPEG_LDFLAGS ${FFMPEG_LDFLAGS} -lbz2 -ltiff -framework DiskArbitration -lfreetype -lfontconfig -llzma -lxml2 -lharfbuzz)
        endif()
    elseif(WIN32)
        set(WIN32_MEDIA_FOUNDATION_LIBS dxva2 evr mf mfplat mfplay mfreadwrite mfuuid amstrmid)
        list(APPEND FFMPEG_LIBRARIES secur32 bcrypt ${WIN32_MEDIA_FOUNDATION_LIBS})

        if(MSYS AND VBAM_STATIC)
            foreach(lib tiff jbig lzma)
                cygpath(lib "$ENV{MSYSTEM_PREFIX}/lib/lib${lib}.a")

                list(APPEND FFMPEG_LIBRARIES "${lib}")
            endforeach()
        endif()
    endif()
else()
    add_definitions(-DNO_FFMPEG)
endif()

if(NOT ENABLE_ONLINEUPDATES)
  add_definitions(-DNO_ONLINEUPDATES)
endif()

if(ENABLE_LIRC)
    set(WITHLIRC 1)
else()
    set(WITHLIRC 0)
endif()

# C defines
add_definitions(-DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_ZLIB_H -DFINAL_VERSION -DSDL -DUSE_OPENGL -DSYSCONF_INSTALL_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}" -DWITH_LIRC=${WITHLIRC})
add_definitions(-DPKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/vbam" -DPACKAGE=)

add_definitions(-D__STDC_FORMAT_MACROS)

# For C++, default to nonstd::optional and nonstd::variant for now due to mac
# build issues.
if(APPLE)
    add_definitions(-Doptional_CONFIG_SELECT_OPTIONAL=optional_OPTIONAL_NONSTD)
    add_definitions(-Dvariant_CONFIG_SELECT_VARIANT=variant_VARIANT_NONSTD)
else()
    add_definitions(-Doptional_CONFIG_SELECT_OPTIONAL=optional_OPTIONAL_STD)
    add_definitions(-Dvariant_CONFIG_SELECT_VARIANT=variant_VARIANT_STD)
endif()

if(ENABLE_LINK)
    # IPC linking code needs sem_timedwait which can be either in librt or pthreads
    if(NOT WIN32)
        find_library(RT_LIB rt)
        if(RT_LIB)
           set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${RT_LIB})
           set(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${RT_LIB})
        endif()
    endif()

    include(CheckFunctionExists)
    check_function_exists(sem_timedwait SEM_TIMEDWAIT)
    if(SEM_TIMEDWAIT)
        add_definitions(-DHAVE_SEM_TIMEDWAIT)
    endif()
else()
    add_definitions(-DNO_LINK)
endif()

# The debugger is enabled by default
if(NOT ENABLE_DEBUGGER)
    add_definitions(-DNO_DEBUGGER)
else()
    add_definitions(-DBKPT_SUPPORT)
endif()

# The ASM core is disabled by default because we don't know on which platform we are
if(NOT ENABLE_ASM_CORE)
    add_definitions(-DC_CORE)
endif()

# Enable internationalization
if(ENABLE_NLS)
    set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    add_definitions(-DENABLE_NLS)
    add_definitions(-DLOCALEDIR="${LOCALEDIR}")
    # for now, only GBALink.cpp uses gettext() directly
    if(APPLE)
        # use Homebrew gettext if available
        if(EXISTS "/usr/local/opt/gettext")
            set(CMAKE_INCLUDE_PATH "${CMAKE_INCLUDE_PATH};/usr/local/opt/gettext/include")
            set(CMAKE_LIBRARY_PATH "${CMAKE_LIBRARY_PATH};/usr/local/opt/gettext/lib")
            set(CMAKE_PROGRAM_PATH "${CMAKE_PROGRAM_PATH};/usr/local/opt/gettext/bin")
        endif()
    endif()
    if(ENABLE_LINK OR ENABLE_WX)
        find_path(LIBINTL_INC libintl.h)
        find_library(LIBINTL_LIB    NAMES libintl    intl)
        find_library(LIBICONV_LIB   NAMES libiconv   iconv)
        find_library(LIBCHARSET_LIB NAMES libcharset charset)
        if(LIBINTL_LIB)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBINTL_LIB})
            list(APPEND VBAMCORE_LIBS            ${LIBINTL_LIB})
        endif()
        if(LIBICONV_LIB)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBICONV_LIB})
            list(APPEND VBAMCORE_LIBS            ${LIBICONV_LIB})
        endif()
        if(LIBCHARSET_LIB)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBCHARSET_LIB})
            list(APPEND VBAMCORE_LIBS            ${LIBCHARSET_LIB})
        endif()
        include(CheckFunctionExists)
        check_function_exists(gettext GETTEXT_FN)
        if(NOT (LIBINTL_INC OR GETTEXT_FN))
            message(FATAL_ERROR "NLS requires libintl/gettext")
        endif()
        include_directories(${LIBINTL_INC})
    endif()
endif()

include(Toolchain)

if(APPLE)
    add_definitions(-DMACHO)
elseif("${CMAKE_SYSTEM}" MATCHES "Linux")
    add_definitions(-DELF)
endif()

if(X86_64)
    add_definitions(-D__AMD64__ -D__X86_64__)
endif()

if(NOT TRANSLATIONS_ONLY)
    add_subdirectory(fex)
endif()

set(
    SRC_MAIN
    src/Util.cpp
    src/common/dictionary.c
    src/common/iniparser.c
    src/common/Patch.cpp
    src/common/memgzio.c
    src/common/SoundSDL.cpp
)

if(MSVC)
    set(SRC_MAIN ${SRC_MAIN} "dependencies/msvc/getopt.c")
endif()

set(
    HDR_MAIN
    src/System.h
    src/Util.h
    src/common/array.h
    src/common/dictionary.h
    src/common/iniparser.h
    src/common/memgzio.h
    src/common/Port.h
    src/common/sizes.h
    src/common/SoundDriver.h
    src/common/SoundSDL.h
)

if(MSVC)
    set(HDR_MAIN ${HDR_MAIN} "dependencies/msvc/getopt.h")
endif()

if(ENABLE_FFMPEG)
    set(SRC_MAIN ${SRC_MAIN} src/common/ffmpeg.cpp)
    set(HDR_MAIN ${HDR_MAIN} src/common/ffmpeg.h)
endif()

if(ENABLE_NLS)
    set(HDR_MAIN ${HDR_MAIN} src/NLS.h)
endif()

set(
    SRC_GBA
    src/gba/agbprint.cpp
    src/gba/bios.cpp
    src/gba/Cheats.cpp
    src/gba/CheatSearch.cpp
    # Built with flex -o debugger-expr-lex.cpp -P dexp_ debugger-expr.l
    src/gba/debugger-expr-lex.cpp
    # Built with bison -L c -o debugger-expr-yacc.cpp -H -p dexp_ debugger-expr.y
    # The yyerrorlab label handler was manually commented out to silence a
    # compiler warning.
    src/gba/debugger-expr-yacc.cpp
    src/gba/EEprom.cpp
    src/gba/ereader.cpp
    src/gba/Flash.cpp
    src/gba/GBA.cpp
    src/gba/GBAGfx.cpp
    src/gba/GBALink.cpp
    src/gba/GBASockClient.cpp
    src/gba/GBA-thumb.cpp
    src/gba/GBA-arm.cpp
    src/gba/gbafilter.cpp
    src/gba/Globals.cpp
    src/gba/Mode0.cpp
    src/gba/Mode1.cpp
    src/gba/Mode2.cpp
    src/gba/Mode3.cpp
    src/gba/Mode4.cpp
    src/gba/Mode5.cpp
    src/gba/RTC.cpp
    src/gba/Sound.cpp
    src/gba/Sram.cpp
)

if(ENABLE_DEBUGGER)
    list(APPEND SRC_GBA
        src/gba/BreakpointStructures.cpp
    )
endif()

set(
    HDR_GBA
    src/gba/agbprint.h
    src/gba/bios.h
    src/gba/BreakpointStructures.h
    src/gba/Cheats.h
    src/gba/CheatSearch.h
    src/gba/debugger-expr-yacc.hpp
    src/gba/EEprom.h
    src/gba/ereader.h
    src/gba/Flash.h
    src/gba/GBA.h
    src/gba/GBAcpu.h
    src/gba/gbafilter.h
    src/gba/GBAGfx.h
    src/gba/GBAinline.h
    src/gba/GBALink.h
    src/gba/GBASockClient.h
    src/gba/Globals.h
    src/gba/RTC.h
    src/gba/Sound.h
    src/gba/Sram.h
)

set(
    SRC_GB
    src/gb/GB.cpp
    src/gb/gbCartData.cpp
    src/gb/gbCheats.cpp
    src/gb/gbDis.cpp
    src/gb/gbGfx.cpp
    src/gb/gbGlobals.cpp
    src/gb/gbMemory.cpp
    src/gb/gbPrinter.cpp
    src/gb/gbSGB.cpp
    src/gb/gbSound.cpp
)

set(
    HDR_GB
    src/gb/gb.h
    src/gb/gbCartData.h
    src/gb/gbCheats.h
    src/gb/gbCodes.h
    src/gb/gbCodesCB.h
    src/gb/gbGlobals.h
    src/gb/gbMemory.h
    src/gb/gbPrinter.h
    src/gb/gbSGB.h
    src/gb/gbSound.h
)

set(
    SRC_APU
    src/apu/Blip_Buffer.cpp
    src/apu/Effects_Buffer.cpp
    src/apu/Gb_Apu.cpp
    src/apu/Gb_Apu_State.cpp
    src/apu/Gb_Oscs.cpp
    src/apu/Multi_Buffer.cpp
)

set(
    HDR_APU
    src/apu/blargg_common.h #Unused(?) but in the VS projects
    src/apu/blargg_config.h #Unused(?) but in the VS projects
    src/apu/blargg_source.h
    src/apu/Blip_Buffer.h
    src/apu/Effects_Buffer.h
    src/apu/Gb_Apu.h
    src/apu/Gb_Oscs.h
    src/apu/Multi_Buffer.h
)

set(
    SRC_SDL
    src/sdl/ConfigManager.cpp
    src/sdl/SDL.cpp
    src/sdl/filters.cpp
    src/sdl/text.cpp
    src/sdl/inputSDL.cpp
    src/sdl/expr.cpp
    src/sdl/exprNode.cpp
    src/sdl/expr-lex.cpp
    src/common/version.cpp
)

set(
    HDR_SDL
    src/sdl/ConfigManager.h
    src/sdl/filters.h
    src/sdl/text.h
    src/sdl/inputSDL.h
    src/sdl/expr.cpp.h
    src/sdl/exprNode.h
    src/common/version_cpp.h
)

set(
    SRC_FILTERS
    src/filters/2xSaI.cpp
    src/filters/admame.cpp
    src/filters/bilinear.cpp
    src/filters/hq2x.cpp
    src/filters/interframe.cpp
    src/filters/pixel.cpp
    src/filters/scanline.cpp
    src/filters/simpleFilter.cpp
    src/filters/xbrzfilter.cpp
    src/filters/xBRZ/xbrz.cpp
)

set(
    HDR_FILTERS
    src/filters/hq2x.h
    src/filters/interp.h
    src/filters/lq2x.h
    src/filters/xBRZ/xbrz_config.h
    src/filters/xBRZ/xbrz.h
    src/filters/xBRZ/xbrz_tools.h
)

set(
    SRC_HQ_C
    src/filters/hq/c/hq_implementation.cpp
)

set(
    HDR_HQ_C
    src/filters/hq/c/hq3x_pattern.h
    src/filters/hq/c/hq4x_pattern.h
    src/filters/hq/c/hq_base.h
    src/filters/hq/c/hq_shared.h
)

set(
    SRC_HQ_ASM
    src/filters/hq/asm/hq3x_16.asm
    src/filters/hq/asm/hq3x_32.asm
    src/filters/hq/asm/hq4x_16.asm
    src/filters/hq/asm/hq4x_32.asm
    src/filters/hq/asm/hq3x32.cpp
)

if(ENABLE_ASM_SCALERS)
    set(SRC_FILTERS ${SRC_FILTERS} ${SRC_HQ_ASM})
    if(ENABLE_MMX)
        set(SRC_FILTERS ${SRC_FILTERS} src/filters/2xSaImmx.asm)
    endif()
else()
    set(SRC_FILTERS ${SRC_FILTERS} ${SRC_HQ_C})
    set(HDR_FILTERS ${HDR_FILTERS} ${HDR_HQ_C})
    add_definitions(-DNO_ASM)
endif()


set(
    SRC_DEBUGGER
    src/gba/armdis.cpp
    src/gba/elf.cpp
)

set(
    HDR_DEBUGGER
    src/gba/armdis.h
    src/gba/elf.h
)

if(ENABLE_DEBUGGER)
    set(
        SRC_DEBUGGER
        ${SRC_DEBUGGER}
        src/gba/remote.cpp
    )
    set(
        HDR_DEBUGGER
        ${HDR_DEBUGGER}
        src/gba/remote.h
    )
endif()

set(
    HDR_STB_IMAGE
    third_party/include/stb/stb_image.h
    third_party/include/stb/stb_image_write.h
)

include_directories(
    ${ZLIB_INCLUDE_DIR}
    fex
    third_party/include
    third_party/include/stb
)

if(ENABLE_FFMPEG)
    include_directories(
        ${FFMPEG_INCLUDE_DIRS}
    )
endif()

if(NOT TRANSLATIONS_ONLY)
    add_library(
        vbamcore
        STATIC
        ${PROJECT_SRCS}
        ${SRC_MAIN}
        ${HDR_MAIN}
        ${SRC_GBA}
        ${HDR_GBA}
        ${SRC_GB}
        ${HDR_GB}
        ${SRC_APU}
        ${HDR_APU}
        ${SRC_FILTERS}
        ${HDR_FILTERS}
        ${SRC_DEBUGGER}
        ${HDR_DEBUGGER}
        ${HDR_STB_IMAGE}
    )
    add_dependencies(vbamcore generate)
    target_include_directories(vbamcore PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

if((NOT TRANSLATIONS_ONLY) AND ENABLE_SDL)
    add_executable(
        vbam
        WIN32
        ${SRC_SDL}
        ${HDR_SDL}
    )

    if(WIN32)
        set(WIN32_LIBRARIES wsock32 ws2_32 winmm version imm32)
    endif()

    if(MSVC)
        # the debug lib libcmtd is linked in debug mode, so don't link the normal version
        set_target_properties(vbam PROPERTIES LINK_FLAGS_DEBUG "/nodefaultlib:libcmt")
    endif()

    if(ENABLE_LIRC)
        set(LIRC_CLIENT_LIBRARY lirc_client)
    endif()

    target_link_libraries(
        vbam
        ${VBAMCORE_LIBS}
        ${WIN32_LIBRARIES}
        ${LIRC_CLIENT_LIBRARY}
    )

    if(WIN32)
        target_link_libraries(vbam ${SDL2MAIN_LIBRARY})
    endif()

    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vbam${CMAKE_EXECUTABLE_SUFFIX} DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

    if(WIN32)
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
            DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}
            RENAME vbam.cfg
        )
    else()
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/vbam.cfg-example
            DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}
            RENAME vbam.cfg
        )
    endif()
endif()

if(ENABLE_WX)
    add_subdirectory(src/wx)
endif()

if(ENABLE_WX)
    # Native Language Support
    if(ENABLE_NLS)
        add_subdirectory(po)
    endif()
endif()
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/vba-over.ini DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/vbam)

# manual pages
if(UNIX)
    if(ENABLE_SDL)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/vbam.6 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man6)
    endif()
    if(ENABLE_WX)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/debian/visualboyadvance-m.6 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man6)
    endif()
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0-Git-${COMMITHASH}")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
include(CPack)

# vim:sw=4 et tw=0:
