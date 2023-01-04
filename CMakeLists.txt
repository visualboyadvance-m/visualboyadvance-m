cmake_minimum_required(VERSION 3.11...3.25) # GLVND was added in 3.10
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW) # use Package_ROOT if set 3.12
endif()
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW) # use vars for options 3.13
endif()

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake")

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type")
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|Debug|RelWithDebInfo|MinSizeRel)$")
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}', must be one of: 'Release', 'Debug', 'RelWithDebInfo' or 'MinSizeRel'")
endif()

if(WIN32)
    include(RemoveStrawberryPerlFromPATH)
endif()

if(UPDATE_APPCAST)
    include(UpdateAppcast)
endif()

if(TAG_RELEASE)
    include(MakeReleaseCommitAndTag)
endif()

## Setup VCPKG
if(ENABLE_LINK)
    list(APPEND VCPKG_MANIFEST_FEATURES sfml)
endif()
if(ENABLE_FFMPEG)
    list(APPEND VCPKG_MANIFEST_FEATURES ffmpeg)
endif()
if(ENABLE_NLS)
    list(APPEND VCPKG_MANIFEST_FEATURES nls)
endif()
if(ENABLE_ONLINEUPDATES)
    list(APPEND VCPKG_MANIFEST_FEATURES update)
endif()
set(VCPKG_INSTALL_OPTIONS --allow-unsupported)
set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
#set(VCPKG_INSTALL_OPTIONS --no-print-usage)
include(Set-Toolchain-vcpkg)
set(VBAM_STATIC_DEFAULT OFF)
if(VCPKG_TARGET_TRIPLET MATCHES "-static-?" OR CMAKE_TOOLCHAIN_FILE MATCHES "mxe|-static-?")
    set(VBAM_STATIC_DEFAULT ON)
endif()
####

include(enable-ccache.cmake)
include(VbamFunctions)

project(VBA-M C CXX)

string(APPEND CMAKE_C_FLAGS_RELEASE " /Zi")
string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG")

include(CTest)
if(BUILD_TESTING)
    enable_testing()
endif()

add_custom_target(generate)

#Output all binaries at top level
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")


if(VBAM_STATIC)
    set(SDL2_STATIC ON)
    set(SFML_STATIC_LIBRARIES ON)
    set(FFMPEG_STATIC ON)
    set(SSP_STATIC ON)
    set(OPENAL_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_START_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_END_STATIC   ON)
endif()

find_package(Git)
find_package(PkgConfig)

# Look for some dependencies using CMake scripts
find_package(ZLIB REQUIRED)

set(OpenGL_GL_PREFERENCE GLVND)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(OpenGL_GL_PREFERENCE LEGACY)
endif()

find_package(OpenGL REQUIRED)
find_package(SDL2   CONFIG REQUIRED)

# msys2 does not have static sfml libs atm
# while on mxe we use static libs
if(WIN32 AND ((NOT (MINGW AND MSYS)) OR CMAKE_TOOLCHAIN_FILE MATCHES mxe) AND NOT CMAKE_TOOLCHAIN_FILE MATCHES vcpkg)
    set(SFML_STATIC_LIBRARIES TRUE)
endif()

if(ENABLE_LINK)
    find_package(SFML 2.4 COMPONENTS network system CONFIG REQUIRED)
endif()

set(FFMPEG_COMPONENTS         AVCODEC            AVFORMAT            SWSCALE          AVUTIL            SWRESAMPLE)
set(FFMPEG_COMPONENT_VERSIONS AVCODEC>=58.18.100 AVFORMAT>=58.12.100 SWSCALE>=5.1.100 AVUTIL>=56.14.100 SWRESAMPLE>=3.1.100)
macro(check_ffmpeg_component_versions)
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
endmacro()

if(ENABLE_FFMPEG)
    find_package(FFmpeg COMPONENTS ${FFMPEG_COMPONENTS} REQUIRED)
    check_ffmpeg_component_versions()
endif()

include(options.cmake)
include(Architecture)
if(ENABLE_ASM_CORE)
    message(WARNING "!!!!!! The x86 ASM cores are considered buggy and dangerous, use at your own risk. !!!!!!")
endif()

if(NOT HTTPS)
    add_definitions(-DNO_HTTPS)
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

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
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
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            )
        endforeach()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} fetch --tags origin
            OUTPUT_QUIET
            ERROR_QUIET
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()
endif()

# no git or no tags, use ChangeLog
if(NOT VERSION)
    include(ChangeLogVersion)
    changelog_version(VERSION REVISION VERSION_RELEASE)
endif()

# generate version.h
include_directories("${CMAKE_BINARY_DIR}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1,\\2,\\3,0" WIN_VERSION "${VERSION}")
configure_file("${CMAKE_SOURCE_DIR}/src/version.h.in" "${CMAKE_BINARY_DIR}/version.h" @ONLY)

add_definitions(-D$<$<NOT:$<CONFIG:DEBUG>>:N>DEBUG)

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
            COMMAND nuget.exe install nasm2 -OutputDirectory "${CMAKE_BINARY_DIR}/nuget"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        )

        file(GLOB pkg ${CMAKE_BINARY_DIR}/nuget/nasm2*)

        list(APPEND CMAKE_PROGRAM_PATH ${pkg}/tools)
    endif()

    enable_language(ASM_NASM)

    set(ASM_ENABLED ON)
endif()

# set the standard libraries all ports use
set(VBAMCORE_LIBS
    vbamcore
    fex
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    sfml-system
    sfml-network
    OpenGL::GL
    ZLIB::ZLIB)

if(ENABLE_FFMPEG)
    if(APPLE)
        set(FFMPEG_LDFLAGS ${FFMPEG_LDFLAGS} -framework CoreText -framework ApplicationServices)
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

include(GNUInstallDirs)

# C defines
add_definitions(-DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_ZLIB_H -DFINAL_VERSION -DSDL -DUSE_OPENGL -DSYSCONF_INSTALL_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}" -DWITH_LIRC=${WITHLIRC})
add_definitions(-DPKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/vbam" -DPACKAGE=)

add_definitions(-D__STDC_FORMAT_MACROS)

# For C++, default to nonstd::optional and nonstd::variant for now due to mac
# build issues.
add_definitions(-Doptional_CONFIG_SELECT_OPTIONAL=optional_OPTIONAL_NONSTD)
add_definitions(-Dvariant_CONFIG_SELECT_VARIANT=variant_VARIANT_NONSTD)


if(ENABLE_LINK)
    # IPC linking code needs sem_timedwait which can be either in librt or pthreads
    if(NOT WIN32)
        find_library(RT_LIB rt)
        if(RT_LIB)
           set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${RT_LIB})
           set(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${RT_LIB})
        endif()
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
    if(ENABLE_LINK OR ENABLE_WX)
        find_package(Intl REQUIRED)
        find_package(Iconv REQUIRED)
        if(NOT Intl_IS_BUILT_IN)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${Intl_LIBRARIES})
            list(APPEND VBAMCORE_LIBS            ${Intl_LIBRARIES})
        endif()
        if(Iconv_FOUND)
            list(APPEND CMAKE_REQUIRED_LIBRARIES ${Iconv_LIBRARIES})
            list(APPEND VBAMCORE_LIBS            ${Iconv_LIBRARIES})
        endif()
        include_directories(${Intl_INCLUDE_DIRS})
    endif()
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/Flags.cmake")

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

if(WIN32)
    find_library(GETOPT_LIB NAMES getopt)
    set(VBAMCORE_LIBS ${VBAMCORE_LIBS} ${GETOPT_LIB})
endif()

set(
    SRC_MAIN
    src/Util.cpp
    src/common/ConfigManager.cpp
    src/common/dictionary.c
    src/common/iniparser.c
    src/common/Patch.cpp
    src/common/memgzio.c
    src/common/SoundSDL.cpp
)

set(
    HDR_MAIN
    src/System.h
    src/Util.h
    src/common/array.h
    src/common/ConfigManager.h
    src/common/dictionary.h
    src/common/iniparser.h
    src/common/memgzio.h
    src/common/Port.h
    src/common/SoundDriver.h
    src/common/SoundSDL.h
)

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

find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")


include_directories(
    fex
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
    )
    target_include_directories(vbamcore PUBLIC ${STB_INCLUDE_DIRS})
    if(MSVC)
        target_include_directories(vbamcore PUBLIC msvc)
    endif()
    add_dependencies(vbamcore generate)
    target_compile_features(vbamcore PUBLIC cxx_std_17)
    target_link_libraries(vbamcore PUBLIC "${GETOPT_LIB}")
    target_link_libraries(vbamcore PUBLIC "$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>")
    target_link_libraries(vbamcore PUBLIC "sfml-network")
endif()

if((NOT TRANSLATIONS_ONLY) AND ENABLE_SDL)
    add_executable(
        vbam
        WIN32
        ${SRC_SDL}
        ${HDR_SDL}
    )
    target_compile_features(vbamcore PUBLIC cxx_std_17)

    if(WIN32)
        set(WIN32_LIBRARIES wsock32 ws2_32 winmm version imm32)
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
        target_link_libraries(vbam SDL2::SDL2main)
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
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/vba-over.ini" DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/vbam")

# manual pages
if(UNIX)
    if(ENABLE_SDL)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/debian/vbam.6" DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man6")
    endif()
    if(ENABLE_WX)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/debian/visualboyadvance-m.6" DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man6")
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
