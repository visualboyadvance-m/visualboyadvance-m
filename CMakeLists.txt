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

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(TAG_RELEASE)
    include(MakeReleaseCommitAndTag)
endif()

set(VCPKG_DEPS zlib libpng SDL2 SFML gettext wxWidgets)

# appveyor job goes over time limit if building ffmpeg during initial cache
# generation. So build initial cache without ffmpeg.
#if(NOT DEFINED ENV{APPVEYOR})
if(1)
    list(APPEND VCPKG_DEPS ffmpeg)
endif()

include(Set-Toolchain-vcpkg)

set(VBAM_STATIC_DEFAULT OFF)

if(VCPKG_TARGET_TRIPLET MATCHES -static OR CMAKE_TOOLCHAIN_FILE MATCHES "mxe|-static")
    set(VBAM_STATIC_DEFAULT ON)
endif()

project(VBA-M C CXX)

cmake_minimum_required(VERSION 2.8.12)

if(NOT CMAKE_PREFIX_PATH AND (NOT ("$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")))
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build Type" FORCE)
endif()

set(ALL_TARGETS fex visualboyadvance-m vbamcore vbam)

add_custom_target(generate)

#Output all binaries at top level
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

option(ENABLE_SDL "Build the SDL port" OFF)
option(ENABLE_WX "Build the wxWidgets port" ON)
option(ENABLE_DEBUGGER "Enable the debugger" ON)
option(ENABLE_ASAN "Enable -fsanitize=<option>, address by default, requires debug build" OFF)

option(VBAM_STATIC "Try to link all libraries statically" ${VBAM_STATIC_DEFAULT})

if(VBAM_STATIC)
    set(SDL2_STATIC ON)
    set(SFML_STATIC_LIBRARIES ON)
    set(FFMPEG_STATIC ON)
    set(SSP_STATIC ON)
    set(OPENAL_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_START_STATIC ON)
    set_property(GLOBAL PROPERTY LINK_SEARCH_END_STATIC   ON)

    if(NOT MSVC)
        list(INSERT CMAKE_FIND_LIBRARY_SUFFIXES 0 .a)
    endif()
endif()

# use ccache if available, and not already enabled on the command line
# but not with ninja and msys ccache on msys2
if(NOT (WIN32 AND NOT "$ENV{MSYSTEM_PREFIX}" STREQUAL "" AND CMAKE_GENERATOR STREQUAL Ninja))
    if(NOT CMAKE_CXX_COMPILER_LAUNCHER AND NOT CMAKE_C_COMPILER_LAUNCHER)
        find_program(CCACHE_FOUND ccache)
        if(CCACHE_FOUND)
            set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
            set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
        endif(CCACHE_FOUND)
    endif()
endif()

set(SSP_DEFAULT OFF)

option(ENABLE_SSP "Enable gcc stack protector support" ${SSP_DEFAULT})

set(ASM_DEFAULT OFF)

include(Architecture)

option(ENABLE_ASM "Enable x86 ASM related options" ${ASM_DEFAULT})

# The ARM ASM core seems to be very buggy, see #98 and #54. Default to it being
# OFF for the time being, until it is either fixed or replaced.
option(ENABLE_ASM_CORE "Enable x86 ASM CPU cores" OFF)

if(ENABLE_ASM_CORE)
    message(WARNING "!!!!!! The x86 ASM cores are considered buggy and dangerous, use at your own risk. !!!!!!")
endif()

set(ASM_SCALERS_DEFAULT ${ENABLE_ASM})
set(MMX_DEFAULT ${ENABLE_ASM})

option(ENABLE_ASM_SCALERS "Enable x86 ASM graphic filters" ${ASM_SCALERS_DEFAULT})

if(ENABLE_ASM_SCALERS)
    option(ENABLE_MMX "Enable MMX" ${MMX_DEFAULT})
endif()

find_package(PkgConfig)

set(ENABLE_LINK_DEFAULT OFF)

# msys2 does not have static sfml libs atm
# while on mxe we use static libs
if(WIN32 AND ((NOT (MINGW AND MSYS)) OR CMAKE_TOOLCHAIN_FILE MATCHES mxe) AND NOT CMAKE_TOOLCHAIN_FILE MATCHES vcpkg)
    set(SFML_STATIC_LIBRARIES TRUE)
endif()

find_package(SFML 2 COMPONENTS network system)

if(SFML_FOUND)
    set(ENABLE_LINK_DEFAULT ON)
endif()

option(ENABLE_LINK "Enable GBA linking functionality" ${ENABLE_LINK_DEFAULT})

option(ENABLE_LIRC "Enable LIRC support" OFF)

set(FFMPEG_DEFAULT OFF)

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

if(((NOT DEFINED ENABLE_FFMPEG) AND (NOT (WIN32 AND X86))) OR ENABLE_FFMPEG)
    set(FFMPEG_DEFAULT ON)

    find_package(FFmpeg COMPONENTS ${FFMPEG_COMPONENTS})
    check_ffmpeg_component_versions()

    if(NOT FFMPEG_FOUND)
        set(FFMPEG_DEFAULT OFF)
    endif()
endif()

option(ENABLE_FFMPEG "Enable ffmpeg A/V recording" ${FFMPEG_DEFAULT})

set(ONLINEUPDATES_DEFAULT OFF)

if(WIN32 AND (X64 OR X86))
    set(ONLINEUPDATES_DEFAULT ON) # use winsparkle
endif()

option(ENABLE_ONLINEUPDATES "Enable online update checks" ${ONLINEUPDATES_DEFAULT})

set(LTO_DEFAULT ON)

# gcc lto produces buggy binaries for 64 bit mingw
# and we generally don't want it when debugging because it makes linking slow
if(CMAKE_BUILD_TYPE MATCHES Debug OR (WIN32 AND CMAKE_COMPILER_IS_GNUCXX))
    set(LTO_DEFAULT OFF)
endif()

option(ENABLE_LTO "Compile with Link Time Optimization (gcc and clang only)" ${LTO_DEFAULT})

if(WIN32 OR CMAKE_TOOLCHAIN_FILE MATCHES "[Mm][Ii][Nn][Gg][Ww]")
    # Win32 deps submodule
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        set(git_checkout FALSE)

        if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
            set(git_checkout TRUE)
            execute_process(
                COMMAND git submodule update --init --remote --recursive
                RESULT_VARIABLE git_status
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            )
        endif()

        if(NOT (git_checkout AND git_status EQUAL 0))
            message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --init --remote --recursive")
        endif()
    endif()
endif()

if(MSVC)
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/msvc")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
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

find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    include(GitTagVersion)
    git_version(VERSION REVISION VERSION_RELEASE)

    # only use the plugin to tie the configure state to the sha to force rebuilds
    # of files that depend on version.h
    include(GetGitRevisionDescription)
    get_git_head_revision(REFSPEC COMMITHASH)
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

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
else()
    add_definitions(-DNDEBUG)
endif()

# hack for ninja in msys2
if(WIN32 AND CMAKE_GENERATOR STREQUAL Ninja AND NOT "$ENV{MSYSTEM_PREFIX}" STREQUAL "")
    set(MSYS ON)
endif()

# Add support for Homebrew, MacPorts and Fink on OS X
if(APPLE)
    include(MacPackageManagers)
endif()

# We do not support amd64 asm yet
if(AMD64 AND (ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS OR ENABLE_MMX))
    message(FATAL_ERROR "The options ASM_CORE, ASM_SCALERS and MMX are not supported on AMD64 yet.")
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

# Look for some dependencies using CMake scripts
find_package(ZLIB REQUIRED)

set(OpenGL_GL_PREFERENCE GLVND)

if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    set(OpenGL_GL_PREFERENCE LEGACY)
endif()

find_package(OpenGL REQUIRED)
find_package(PNG    REQUIRED)
find_package(SDL2   REQUIRED)

if(WIN32)
    set(SDL2_LIBRARY ${SDL2_LIBRARY} setupapi)
endif()

# set the standard libraries all ports use
set(
    VBAMCORE_LIBS
    vbamcore
    fex
    ${SDL2_LIBRARY}
    ${SFML_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${PNG_LIBRARY}
)

if(ENABLE_FFMPEG)
    if(NOT FFMPEG_LIBRARIES)
        message(FATAL_ERROR "ENABLE_FFMPEG was specified, but required versions of ffmpeg libraries cannot be found!")
    endif()

    if(APPLE)
        set(FFMPEG_LDFLAGS ${FFMPEG_LDFLAGS} -framework CoreText -framework ApplicationServices)
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
       set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} -lpthread)
       set(VBAMCORE_LIBS ${VBAMCORE_LIBS} -lpthread)
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

set(NLS_DEFAULT ON)

option(ENABLE_NLS "Enable translations" ${NLS_DEFAULT})

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

include(ProcessorCount)
ProcessorCount(num_cpus)

# Compiler stuff

if(CMAKE_C_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    include(LLVMToolchain)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    unset(LTO_FLAGS)
    if(ENABLE_LTO)
        if(CMAKE_COMPILER_IS_GNUCXX)
            if(num_cpus GREATER 1)
                set(LTO_FLAGS -flto=${num_cpus} -ffat-lto-objects)
            else()
                set(LTO_FLAGS -flto -ffat-lto-objects)
            endif()
        else()
            set(LTO_FLAGS -flto)
        endif()

        # check that LTO is not broken before enabling it
        set(ORIG_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${LTO_FLAGS}")

        include(CheckCXXSourceCompiles)
        check_cxx_source_compiles("int main(int argc, char** argv) { return 0; }" LTO_WORKS)

        set(CMAKE_REQUIRED_FLAGS ${ORIG_CMAKE_REQUIRED_FLAGS})

        if(NOT LTO_WORKS)
            message(WARNING "LTO does not seem to be working on your system, if using clang make sure LLVMGold is installed")
            unset(LTO_FLAGS)
            set(ENABLE_LTO OFF)
        endif()
    endif()

    unset(MY_C_OPT_FLAGS)

    if(X86_32 OR AMD64)
        set(MY_C_OPT_FLAGS -mfpmath=sse -msse2)
    endif()

    # common optimization flags
    if(NOT (APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3))
        set(MY_C_OPT_FLAGS ${MY_C_OPT_FLAGS} -O2 -fomit-frame-pointer ${LTO_FLAGS})
    else()
        # LTO and -fomit-frame-pointer generate broken binaries on Lion with XCode 4.2 tools
        set(MY_C_OPT_FLAGS ${MY_C_OPT_FLAGS} -O2)
    endif()

    # only use -mtune=generic on PC architectures
    if(X86_32 OR AMD64)
        set(MY_C_OPT_FLAGS ${MY_C_OPT_FLAGS} -mtune=generic)
    endif()

    # common debug flags
    if(CMAKE_COMPILER_IS_GNUCXX)
        set(MY_C_DBG_FLAGS -ggdb3 -Og -fno-omit-frame-pointer)
    else()
        set(MY_C_DBG_FLAGS -g -fno-omit-frame-pointer)
    endif()

    include(CheckCXXCompilerFlag)

    if(ENABLE_ASAN)
        string(TOLOWER ${CMAKE_BUILD_TYPE} build)
        if(NOT build STREQUAL "debug")
            message(FATAL_ERROR "asan requires debug build, set -DCMAKE_BUILD_TYPE=Debug")
        endif()

        string(TOLOWER ${ENABLE_ASAN} SANITIZER)
        if(SANITIZER STREQUAL "on" OR SANITIZER STREQUAL "true")
            set(SANITIZER address)
        endif()
        list(PREPEND CMAKE_REQUIRED_LIBRARIES -fsanitize=${SANITIZER})
        check_cxx_compiler_flag("-fsanitize=${SANITIZER}" ASAN_SUPPORT_FLAG)
        if(${ASAN_SUPPORT_FLAG})
            list(PREPEND MY_C_DBG_FLAGS -fsanitize=${SANITIZER})
        else()
            message(FATAL_ERROR "asan not available to compiler.")
        endif()
    endif()

    # common flags
    set(MY_C_FLAGS -pipe -Wformat -Wformat-security -feliminate-unused-debug-types)

    if(ENABLE_SSP AND CMAKE_BUILD_TYPE STREQUAL Debug)
        if(WIN32)
            set(SSP_STATIC ON)
        endif()

        find_package(SSP)

        if(SSP_LIBRARY)
            list(APPEND MY_C_FLAGS        -D_FORTIFY_SOURCE=2)
            list(APPEND MY_C_LINKER_FLAGS ${SSP_LIBRARY})
        endif()
    endif()

    if(NOT (WIN32 OR X86_32)) # inline asm is not allowed with -fPIC
        set(MY_C_FLAGS ${MY_C_FLAGS} -fPIC)
    endif()

    # check if ssp flags are supported for this version of gcc
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(ENABLE_SSP)
            check_cxx_compiler_flag(-fstack-protector-strong  F_STACK_PROTECTOR_STRONG_FLAG)

            if(F_STACK_PROTECTOR_STRONG_FLAG)
                set(MY_C_FLAGS ${MY_C_FLAGS} -fstack-protector-strong)

                check_cxx_compiler_flag("--param ssp-buffer-size=4" SSP_BUFFER_SIZE_FLAG)

                if(SSP_BUFFER_SIZE_FLAG)
                    # we do not add it to MY_C_FLAGS because this breaks things like CMAKE_REQUIRED_LIBRARIES
                    # which misinterpret compiler flags without leading dashes
                    add_compile_options(--param ssp-buffer-size=4)
                endif()
            endif()
        endif()

        set(MY_C_FLAGS ${MY_C_FLAGS} -fopenmp)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
        unset(COMPILER_COLOR_DIAGNOSTICS)
        check_cxx_compiler_flag(-fdiagnostics-color=always COMPILER_COLOR_DIAGNOSTICS)
        if(COMPILER_COLOR_DIAGNOSTICS)
            add_compile_options(-fdiagnostics-color=always)
        else()
            unset(COMPILER_COLOR_DIAGNOSTICS)
            check_cxx_compiler_flag(-fdiagnostics-color COMPILER_COLOR_DIAGNOSTICS)
            if(COMPILER_COLOR_DIAGNOSTICS)
                add_compile_options(-fdiagnostics-color)
            endif()
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        unset(COMPILER_COLOR_DIAGNOSTICS)
        check_cxx_compiler_flag(-fcolor-diagnostics COMPILER_COLOR_DIAGNOSTICS)
        if(COMPILER_COLOR_DIAGNOSTICS)
            add_compile_options(-fcolor-diagnostics)
        endif()
    endif()

    if(MINGW)
        set(MY_C_FLAGS ${MY_C_FLAGS} -static-libgcc -static-libstdc++)
    endif()

    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(MY_C_FLAGS   ${MY_C_FLAGS}   ${MY_C_DBG_FLAGS} -Wall -Wextra)
    else()
        set(MY_C_FLAGS   ${MY_C_FLAGS}   ${MY_C_OPT_FLAGS} -Wno-error)
    endif()

    # for some reason this is necessary
    if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
        set(MY_C_FLAGS -I/usr/local/include ${MY_C_FLAGS})
    endif()

    foreach(C_COMPILE_FLAG ${MY_C_FLAGS})
        add_compile_options(${C_COMPILE_FLAG})
    endforeach()

    include(CheckCXXCompilerFlag)

    set(MY_CXX_FLAGS -std=gnu++11 -fexceptions)

    foreach(ARG ${MY_CXX_FLAGS})
        set(MY_CXX_FLAGS_STR "${MY_CXX_FLAGS_STR} ${ARG}")
    endforeach()

    # These must be set for C++ only, and we can't use generator expressions in
    # ADD_COMPILE_OPTIONS because that's a cmake 3.3 feature and we need 2.8.12
    # compat for Ubuntu 14.
    string(REGEX REPLACE "<FLAGS>" "<FLAGS> ${MY_CXX_FLAGS_STR} " CMAKE_CXX_COMPILE_OBJECT ${CMAKE_CXX_COMPILE_OBJECT})

    foreach(ARG ${MY_C_FLAGS})
        set(MY_C_FLAGS_STR "${MY_C_FLAGS_STR} ${ARG}")
    endforeach()

    # need all flags for linking, because of -flto etc.
    set(CMAKE_C_LINK_EXECUTABLE   "${CMAKE_C_LINK_EXECUTABLE}   ${MY_C_FLAGS_STR}")
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${MY_C_FLAGS_STR}")

    # for the gcc -fstack-protector* flags we need libssp
    # we also have to use the gcc- binutils for LTO to work
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(ENABLE_LTO)
            include(UseGCCBinUtilsWrappers)
        endif()

        set(MY_C_LINKER_FLAGS ${MY_C_LINKER_FLAGS} -Wl,-allow-multiple-definition)

        if(CMAKE_PREFIX_PATH)
            list(GET CMAKE_PREFIX_PATH 0 prefix_path_first)
            set(MY_C_LINKER_FLAGS ${MY_C_LINKER_FLAGS} "-Wl,-rpath-link=${prefix_path_first}/lib")
        endif()
    endif()

    # set linker flags
    foreach(ARG ${MY_C_LINKER_FLAGS})
        set(MY_C_LINKER_FLAGS_STR "${MY_C_LINKER_FLAGS_STR} ${ARG}")
    endforeach()

    set(CMAKE_C_LINK_EXECUTABLE   "${CMAKE_C_LINK_EXECUTABLE}   ${MY_C_LINKER_FLAGS_STR}")
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${MY_C_LINKER_FLAGS_STR}")
elseif(MSVC)
    # first remove all warnings flags, otherwise there is a warning about overriding them
    string(REGEX REPLACE "/[Ww][^ ]+" "" CMAKE_C_FLAGS   ${CMAKE_C_FLAGS})
    string(REGEX REPLACE "/[Ww][^ ]+" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        add_compile_options(/W4)
    else()
        add_compile_options(/w)

        if(ENABLE_LTO)
            add_compile_options(/GL)

            set(CMAKE_EXE_LINKER_FLAGS          "${CMAKE_EXE_LINKER_FLAGS}          /LTCG")
            set(CMAKE_SHARED_LINKER_FLAGS       "${CMAKE_SHARED_LINKER_FLAGS}       /LTCG")
            set(CMAKE_MODULE_LINKER_FLAGS       "${CMAKE_MODULE_LINKER_FLAGS}       /LTCG")
            set(CMAKE_STATIC_LINKER_FLAGS       "${CMAKE_STATIC_LINKER_FLAGS}       /LTCG")
        endif()
    endif()

    if(VBAM_STATIC)
        string(REGEX REPLACE "/MDd?" "" CMAKE_C_FLAGS                  ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_C_FLAGS_MINSIZEREL       ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_C_FLAGS_RELEASE          ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_C_FLAGS_RELWITHDEBINFO   ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_C_FLAGS_DEBUG            ${CMAKE_CXX_FLAGS})

        string(REGEX REPLACE "/MDd?" "" CMAKE_CXX_FLAGS                ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_CXX_FLAGS_MINSIZEREL     ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_CXX_FLAGS_RELEASE        ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE "/MDd?" "" CMAKE_CXX_FLAGS_DEBUG          ${CMAKE_CXX_FLAGS})

        if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
            add_compile_options(/MTd)
        else()
            add_compile_options(/MT)
        endif()
    endif()
endif()

# Assembler flags

if(ASM_ENABLED)
    string(REGEX REPLACE "<FLAGS>" "-I${CMAKE_SOURCE_DIR}/src/filters/hq/asm/ -O1 -w-orphan-labels" CMAKE_ASM_NASM_COMPILE_OBJECT ${CMAKE_ASM_NASM_COMPILE_OBJECT})
endif()

if(APPLE)
    add_definitions(-DMACHO)
elseif("${CMAKE_SYSTEM}" MATCHES "Linux")
    add_definitions(-DELF)
endif()

if(AMD64)
    add_definitions(-D__AMD64__)
endif()

add_subdirectory (fex)

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

if(MSVC)
        set(SRC_MAIN ${SRC_MAIN} "dependencies/msvc/getopt.c")
endif()

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
    src/gba/debugger-expr-lex.cpp
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

include_directories(
    ${ZLIB_INCLUDE_DIR}
    fex
    ${PNG_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIR}
)

if(ENABLE_FFMPEG)
    include_directories(
        ${FFMPEG_INCLUDE_DIRS}
    )
endif()

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
add_dependencies(vbamcore generate)
set_property(TARGET vbamcore PROPERTY CXX_STANDARD 11)
set_property(TARGET vbamcore PROPERTY CXX_STANDARD_REQUIRED ON)

if(ENABLE_SDL)
    add_executable(
        vbam
        WIN32
        ${SRC_SDL}
        ${HDR_SDL}
    )
    set_property(TARGET vbam PROPERTY CXX_STANDARD 11)
    set_property(TARGET vbam PROPERTY CXX_STANDARD_REQUIRED ON)

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
set(CPACK_PACKAGE_VERSION_PATCH "0-Git-" ${COMMITHASH})
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
include(CPack)
