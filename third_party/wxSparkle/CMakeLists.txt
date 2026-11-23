#
#  wxSparkle -- a wxWidgets port of WinSparkle (https://winsparkle.org)
#
#  Copyright (C) 2009-2026 Vaclav Slavik
#  SPDX-License-Identifier: MIT (see COPYING)
#

cmake_minimum_required(VERSION 3.16)

# Parse version header to always have actual version of WinSparkle
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/winsparkle-version.h" _version_h)
string(REGEX MATCH "WIN_SPARKLE_VERSION_MAJOR[ \t]+([0-9]+)" _ "${_version_h}")
set(_ver_major ${CMAKE_MATCH_1})
string(REGEX MATCH "WIN_SPARKLE_VERSION_MINOR[ \t]+([0-9]+)" _ "${_version_h}")
set(_ver_minor ${CMAKE_MATCH_1})
string(REGEX MATCH "WIN_SPARKLE_VERSION_MICRO[ \t]+([0-9]+)" _ "${_version_h}")
set(_ver_micro ${CMAKE_MATCH_1})

project(wxSparkle
        VERSION ${_ver_major}.${_ver_minor}.${_ver_micro}
        DESCRIPTION "Software update library for wxWidgets applications (WinSparkle port)"
        LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

# Classic Mac OS (System 7 - Mac OS 9) with the Retro68 toolchain: 68k
# (retro68.toolchain.cmake) or PowerPC (retroppc.toolchain.cmake).
set(WXSPARKLE_RETRO68 OFF)
if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC)$")
    set(WXSPARKLE_RETRO68 ON)

    # The sysroot, e.g. /opt/retro68/m68k-apple-macos
    list(GET CMAKE_SYSTEM_PREFIX_PATH 0 WXSPARKLE_RETRO68_SYSROOT)
    get_filename_component(WXSPARKLE_RETRO68_SYSROOT "${WXSPARKLE_RETRO68_SYSROOT}" ABSOLUTE)

    # Retro68's toolchain files don't confine find_*() to the sysroot, so the
    # host's packages (e.g. libcurl or wx-config in /usr/local) would be found
    # instead of the sysroot's ones. Host programs (msgfmt) are fine.
    list(APPEND CMAKE_FIND_ROOT_PATH "${WXSPARKLE_RETRO68_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    # same for pkg-config, which would look into the host's directories too
    if(NOT DEFINED ENV{PKG_CONFIG_LIBDIR})
        set(ENV{PKG_CONFIG_LIBDIR} "${WXSPARKLE_RETRO68_SYSROOT}/lib/pkgconfig:${WXSPARKLE_RETRO68_SYSROOT}/share/pkgconfig")
    endif()

    # keep the (statically linked) applications small
    add_compile_options(-ffunction-sections -fdata-sections)

    message(STATUS "wxSparkle: building for classic Mac OS (${CMAKE_SYSTEM_NAME}, ${WXSPARKLE_RETRO68_SYSROOT})")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WXSPARKLE_RETRO68)
    # newlib only declares POSIX functions used by wxWidgets' headers (strdup,
    # strnlen, ...) in the GNU mode
    set(CMAKE_CXX_EXTENSIONS ON)
else()
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#---------------------------------------------------------------------------
# Options
#---------------------------------------------------------------------------

option(BUILD_SHARED_LIBS "Build wxSparkle as a shared library (DLL) instead of a static one" OFF)
option(WXSPARKLE_USE_WEBVIEW "Show release notes using wxWebView (if available)" ON)
option(WXSPARKLE_WITH_DSA "Support legacy DSA signatures (requires OpenSSL)" ON)
option(WXSPARKLE_TRUST_NATIVE_CA "Trust the OS certificate store in addition to the embedded CA bundle" OFF)
option(WXSPARKLE_TRANSLATIONS "Embed translations (requires gettext's msgfmt)" ON)
# winsparkle-tool is a command line program to run on the developer's machine
# and the unit tests need a POSIX file system, so neither is built for
# classic Mac OS by default.
if(WXSPARKLE_RETRO68)
    set(_wxsparkle_host_tools_default OFF)
else()
    set(_wxsparkle_host_tools_default ON)
endif()
option(WXSPARKLE_BUILD_EXAMPLE "Build the example wxWidgets application" ON)
option(WXSPARKLE_BUILD_TOOL "Build winsparkle-tool (EdDSA key generation and signing)" ${_wxsparkle_host_tools_default})
option(WXSPARKLE_BUILD_TESTS "Build unit tests" ${_wxsparkle_host_tools_default})

set(WXSPARKLE_CA_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/cert.pem" CACHE FILEPATH
    "PEM file with the CA certificates embedded into the library")

# Built as part of another project (add_subdirectory): don't install the
# library, its headers and package files along with the host application.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_wxsparkle_top_level ON)
else()
    set(_wxsparkle_top_level OFF)
endif()
option(WXSPARKLE_INSTALL "Install wxSparkle (library, headers, CMake package)" ${_wxsparkle_top_level})

# A host application that has already found wxWidgets (in its own way) can
# hand it over as a target, so that wxSparkle is built against exactly the
# same wxWidgets build: set WXSPARKLE_WX_TARGET to a target that carries
# wxWidgets' include directories, definitions, flags and libraries (at least
# html, core and base), and WXSPARKLE_WX_HAS_WEBVIEW if it includes webview.
# (Normal variables, set by the host before add_subdirectory().)
if(NOT DEFINED WXSPARKLE_WX_TARGET)
    set(WXSPARKLE_WX_TARGET "")
endif()

# Classic Mac OS: Retro68's libcurl gets its BSD sockets from libmactcp. A
# host application that has its own libmactcp-compatible socket layer (the
# same calls, structure layouts and constants, mactcp_set_idle_func()
# included) can set WXSPARKLE_MACTCP_TARGET to a target that provides it,
# with the <sys/socket.h> & co. declaring it as its public include
# directories; libcurl is then linked against that target and libmactcp is
# left out, as the two would define the same functions.
if(NOT DEFINED WXSPARKLE_MACTCP_TARGET)
    set(WXSPARKLE_MACTCP_TARGET "")
endif()

#---------------------------------------------------------------------------
# Dependencies
#---------------------------------------------------------------------------

# wxWidgets: prefer CMake config packages (vcpkg, wx built with CMake), fall
# back to FindwxWidgets (wx-config on Unix).
set(_wx_libs)
set(WXSPARKLE_HAVE_WEBVIEW OFF)

if(WXSPARKLE_WX_TARGET)
    if(NOT TARGET ${WXSPARKLE_WX_TARGET})
        message(FATAL_ERROR "WXSPARKLE_WX_TARGET: ${WXSPARKLE_WX_TARGET} is not a target")
    endif()
elseif(WXSPARKLE_RETRO68)
    # wxRetro68's CMake package refers to its dependencies' targets (JPEG::JPEG,
    # ZSTD::ZSTD, WebP::webp, ...) without looking for them, so it can't be
    # used: use the sysroot's wx-config (never the host's one).
    set(wxWidgets_CONFIG_EXECUTABLE "${WXSPARKLE_RETRO68_SYSROOT}/bin/wx-config"
        CACHE FILEPATH "wx-config of wxWidgets for Retro68")
    if(NOT EXISTS "${wxWidgets_CONFIG_EXECUTABLE}")
        message(FATAL_ERROR "wxWidgets for Retro68 (wxRetro68) not found: "
                            "${wxWidgets_CONFIG_EXECUTABLE} doesn't exist")
    endif()
else()
    find_package(wxWidgets 3.2 CONFIG QUIET COMPONENTS core base)
    if(NOT wxWidgets_FOUND)
        # wxWidgets' package version file only accepts requests for the same
        # major.minor version (so that 3.3 doesn't satisfy "3.2"); look for
        # any version and check it here instead.
        find_package(wxWidgets CONFIG QUIET COMPONENTS core base)
        if(wxWidgets_FOUND AND wxWidgets_VERSION VERSION_LESS 3.2)
            message(STATUS "wxSparkle: ignoring wxWidgets ${wxWidgets_VERSION} CMake package (3.2 or newer is required)")
            set(wxWidgets_FOUND FALSE)
        endif()
    endif()
endif()
if(WXSPARKLE_WX_TARGET)
    set(_wx_libs ${WXSPARKLE_WX_TARGET})
    if(WXSPARKLE_USE_WEBVIEW AND WXSPARKLE_WX_HAS_WEBVIEW)
        set(WXSPARKLE_HAVE_WEBVIEW ON)
    endif()
    set(WXSPARKLE_WX_CMAKE_PACKAGE OFF)
    message(STATUS "wxSparkle: using wxWidgets from ${WXSPARKLE_WX_TARGET}")
elseif(wxWidgets_FOUND AND TARGET wx::core)
    set(_wx_libs wx::core wx::base)
    if(TARGET wx::html)
        list(PREPEND _wx_libs wx::html)
    endif()
    if(WXSPARKLE_USE_WEBVIEW AND TARGET wx::webview)
        list(PREPEND _wx_libs wx::webview)
        set(WXSPARKLE_HAVE_WEBVIEW ON)
    endif()
    set(WXSPARKLE_WX_CMAKE_PACKAGE ON)
    message(STATUS "wxSparkle: using wxWidgets ${wxWidgets_VERSION} CMake package")
else()
    # (wxRetro68 has no wxWebView, but its wx-config doesn't complain about it)
    if(WXSPARKLE_USE_WEBVIEW AND NOT WXSPARKLE_RETRO68)
        find_package(wxWidgets 3.2 QUIET COMPONENTS webview html core base)
        if(wxWidgets_FOUND)
            set(WXSPARKLE_HAVE_WEBVIEW ON)
        endif()
    endif()
    if(NOT wxWidgets_FOUND)
        find_package(wxWidgets 3.2 COMPONENTS html core base)
    endif()
    if(NOT wxWidgets_FOUND)
        find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
    endif()

    add_library(wxsparkle_wx INTERFACE)
    target_include_directories(wxsparkle_wx INTERFACE ${wxWidgets_INCLUDE_DIRS})
    target_compile_definitions(wxsparkle_wx INTERFACE ${wxWidgets_DEFINITIONS}
                               $<$<CONFIG:Debug>:${wxWidgets_DEFINITIONS_DEBUG}>)
    if(wxWidgets_CXX_FLAGS)
        separate_arguments(_wx_cxx_flags NATIVE_COMMAND "${wxWidgets_CXX_FLAGS}")
        # include dirs and defines are handled above
        list(FILTER _wx_cxx_flags EXCLUDE REGEX "^-[ID]")
        target_compile_options(wxsparkle_wx INTERFACE ${_wx_cxx_flags})
    endif()
    target_link_libraries(wxsparkle_wx INTERFACE ${wxWidgets_LIBRARIES})
    set(_wx_libs wxsparkle_wx)
    set(WXSPARKLE_WX_CMAKE_PACKAGE OFF)
    message(STATUS "wxSparkle: using FindwxWidgets (${wxWidgets_VERSION_STRING})")
endif()
message(STATUS "wxSparkle: wxWebView release notes: ${WXSPARKLE_HAVE_WEBVIEW}")

# libcurl >= 7.77 for CURLOPT_CAINFO_BLOB
find_package(CURL 7.77 REQUIRED)
if(TARGET CURL::libcurl_static AND NOT TARGET CURL::libcurl)
    set(_curl_target CURL::libcurl_static)
else()
    set(_curl_target CURL::libcurl)
endif()

add_library(wxsparkle_curl INTERFACE)
target_link_libraries(wxsparkle_curl INTERFACE ${_curl_target})
set(_curl_lib wxsparkle_curl)

# A static libcurl's CMake package doesn't always list transitive
# dependencies of its own dependencies (e.g. ICU used by libpsl, or GNU
# libiconv used by libidn2). pkg-config --static knows them, so use it.
set(_curl_real_target ${_curl_target})
get_target_property(_curl_aliased ${_curl_target} ALIASED_TARGET)
if(_curl_aliased)
    set(_curl_real_target ${_curl_aliased})
endif()
get_target_property(_curl_type ${_curl_real_target} TYPE)
# FindCURL's target is an UNKNOWN library, static or not: go by the file.
if(_curl_type STREQUAL "UNKNOWN_LIBRARY")
    foreach(_curl_location_prop IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_DEBUG)
        get_target_property(_curl_location ${_curl_real_target} ${_curl_location_prop})
        if(_curl_location)
            if(_curl_location MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$"
               AND NOT _curl_location MATCHES "\\.dll\\.a$")
                set(_curl_type STATIC_LIBRARY)
            endif()
            break()
        endif()
    endforeach()
endif()
option(WXSPARKLE_CURL_PKGCONFIG_STATIC "Use pkg-config to find static libcurl's dependencies" ON)
if(WXSPARKLE_RETRO68)
    # Retro68's libcurl comes without pkg-config or CMake files. It uses
    # OpenSSL for TLS, zlib for content decoding and MacTCP (libmactcp, which
    # provides BSD sockets) for networking.
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)
    if(WXSPARKLE_MACTCP_TARGET)
        if(NOT TARGET ${WXSPARKLE_MACTCP_TARGET})
            message(FATAL_ERROR "WXSPARKLE_MACTCP_TARGET (${WXSPARKLE_MACTCP_TARGET}) is not a target")
        endif()
        set(_wxsparkle_mactcp ${WXSPARKLE_MACTCP_TARGET})
        message(STATUS "wxSparkle: libcurl's sockets from ${WXSPARKLE_MACTCP_TARGET}")
    else()
        find_library(WXSPARKLE_MACTCP_LIBRARY NAMES mactcp)
        # (its BSD sockets headers are in <include>/mactcp)
        find_path(WXSPARKLE_MACTCP_INCLUDE_PARENT NAMES mactcp/sys/socket.h)
        if(NOT WXSPARKLE_MACTCP_LIBRARY OR NOT WXSPARKLE_MACTCP_INCLUDE_PARENT)
            message(FATAL_ERROR "libmactcp (needed by Retro68's libcurl) not found")
        endif()
        set(WXSPARKLE_MACTCP_INCLUDE_DIR "${WXSPARKLE_MACTCP_INCLUDE_PARENT}/mactcp")
        set(_wxsparkle_mactcp "${WXSPARKLE_MACTCP_LIBRARY}")
    endif()
    target_link_libraries(wxsparkle_curl INTERFACE
        OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB ${_wxsparkle_mactcp})
elseif(_curl_type STREQUAL "STATIC_LIBRARY" AND WXSPARKLE_CURL_PKGCONFIG_STATIC AND NOT MSVC)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(WXSPARKLE_CURL_PC QUIET libcurl)
    endif()
    if(WXSPARKLE_CURL_PC_FOUND)
        # keep "-framework X" pairs together
        set(_curl_static_flags)
        set(_pending_framework OFF)
        foreach(_flag ${WXSPARKLE_CURL_PC_STATIC_LDFLAGS})
            if(_pending_framework)
                list(APPEND _curl_static_flags "-framework ${_flag}")
                set(_pending_framework OFF)
            elseif(_flag STREQUAL "-framework")
                set(_pending_framework ON)
            else()
                list(APPEND _curl_static_flags "${_flag}")
            endif()
        endforeach()
        target_link_libraries(wxsparkle_curl INTERFACE ${_curl_static_flags})
        # some of those may be shared libraries with @rpath install names
        foreach(_dir ${WXSPARKLE_CURL_PC_STATIC_LIBRARY_DIRS})
            if(NOT _dir MATCHES "^/usr/lib")
                # (as a link flag, so that it propagates through the static
                # library's private dependencies)
                target_link_libraries(wxsparkle_curl INTERFACE "-Wl,-rpath,${_dir}")
            endif()
        endforeach()
        message(STATUS "wxSparkle: static libcurl, using pkg-config for its dependencies")
    endif()
endif()

find_package(EXPAT REQUIRED)

set(WXSPARKLE_HAVE_OPENSSL OFF)
if(WXSPARKLE_WITH_DSA)
    find_package(OpenSSL 1.1)
    if(OPENSSL_FOUND)
        set(WXSPARKLE_HAVE_OPENSSL ON)
    else()
        message(WARNING "OpenSSL not found, legacy DSA signatures won't be supported")
    endif()
endif()
message(STATUS "wxSparkle: DSA signature support: ${WXSPARKLE_HAVE_OPENSSL}")

#---------------------------------------------------------------------------
# Generated headers (bin2c)
#---------------------------------------------------------------------------

set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_bin2c "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2c.cmake")
file(MAKE_DIRECTORY "${_gen_dir}")

# CA certificates bundle used by libcurl
if(NOT EXISTS "${WXSPARKLE_CA_BUNDLE}")
    message(FATAL_ERROR "CA bundle ${WXSPARKLE_CA_BUNDLE} does not exist")
endif()
add_custom_command(
    OUTPUT "${_gen_dir}/wxsparkle_cacert.h"
    COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_gen_dir}/wxsparkle_cacert.h"
            "-DINPUTS=${WXSPARKLE_CA_BUNDLE}"
            "-DSYMBOLS=wxsparkle_cacert_pem"
            -DPEM_ONLY=ON
            -P "${_bin2c}"
    DEPENDS "${WXSPARKLE_CA_BUNDLE}" "${_bin2c}"
    COMMENT "Embedding CA certificates from ${WXSPARKLE_CA_BUNDLE}"
    VERBATIM)

# WinSparkle icons
set(_icon_inputs)
set(_icon_symbols)
set(_icon_names)
foreach(_size 16 20 24 32 48 64 256)
    list(APPEND _icon_inputs "${CMAKE_CURRENT_SOURCE_DIR}/artwork/WinSparkle-${_size}x${_size}.png")
    list(APPEND _icon_symbols "wxsparkle_icon_${_size}")
    list(APPEND _icon_names "${_size}")
endforeach()
string(REPLACE ";" "|" _icon_inputs_arg "${_icon_inputs}")
string(REPLACE ";" "|" _icon_symbols_arg "${_icon_symbols}")
string(REPLACE ";" "|" _icon_names_arg "${_icon_names}")
add_custom_command(
    OUTPUT "${_gen_dir}/wxsparkle_icons.h"
    COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_gen_dir}/wxsparkle_icons.h"
            "-DINPUTS=${_icon_inputs_arg}"
            "-DSYMBOLS=${_icon_symbols_arg}"
            "-DNAMES=${_icon_names_arg}"
            -DTABLE=wxsparkle_icons
            -P "${_bin2c}"
    DEPENDS ${_icon_inputs} "${_bin2c}"
    COMMENT "Embedding icons"
    VERBATIM)

# Translations: .po -> .mo -> embedded
set(_mo_files)
set(_mo_symbols)
set(_mo_names)
if(WXSPARKLE_TRANSLATIONS)
    find_package(Gettext)
    if(GETTEXT_MSGFMT_EXECUTABLE)
        file(GLOB _po_files "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.po")
        list(SORT _po_files)
        file(MAKE_DIRECTORY "${_gen_dir}/mo")
        foreach(_po ${_po_files})
            get_filename_component(_lang "${_po}" NAME_WLE)
            set(_mo "${_gen_dir}/mo/${_lang}.mo")
            add_custom_command(
                OUTPUT "${_mo}"
                COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" -c -o "${_mo}" "${_po}"
                DEPENDS "${_po}"
                COMMENT "Compiling translation ${_lang}"
                VERBATIM)
            string(MAKE_C_IDENTIFIER "wxsparkle_mo_${_lang}" _sym)
            list(APPEND _mo_files "${_mo}")
            list(APPEND _mo_symbols "${_sym}")
            list(APPEND _mo_names "${_lang}")
        endforeach()
    else()
        message(WARNING "msgfmt not found, translations won't be embedded")
    endif()
endif()
string(REPLACE ";" "|" _mo_files_arg "${_mo_files}")
string(REPLACE ";" "|" _mo_symbols_arg "${_mo_symbols}")
string(REPLACE ";" "|" _mo_names_arg "${_mo_names}")
add_custom_command(
    OUTPUT "${_gen_dir}/wxsparkle_translations.h"
    COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_gen_dir}/wxsparkle_translations.h"
            "-DINPUTS=${_mo_files_arg}"
            "-DSYMBOLS=${_mo_symbols_arg}"
            "-DNAMES=${_mo_names_arg}"
            -DTABLE=wxsparkle_translations
            -P "${_bin2c}"
    DEPENDS ${_mo_files} "${_bin2c}"
    COMMENT "Embedding translations"
    VERBATIM)

#---------------------------------------------------------------------------
# ed25519 (bundled)
#---------------------------------------------------------------------------

add_library(wxsparkle_ed25519 OBJECT
    3rdparty/ed25519/src/add_scalar.c
    3rdparty/ed25519/src/fe.c
    3rdparty/ed25519/src/ge.c
    3rdparty/ed25519/src/key_exchange.c
    3rdparty/ed25519/src/keypair.c
    3rdparty/ed25519/src/sc.c
    3rdparty/ed25519/src/seed.c
    3rdparty/ed25519/src/sha512.c
    3rdparty/ed25519/src/sign.c
    3rdparty/ed25519/src/verify.c)
target_include_directories(wxsparkle_ed25519 PUBLIC 3rdparty/ed25519/src)
# (classic Mac OS code is relocated by the loader, -fPIC isn't supported)
if(NOT WXSPARKLE_RETRO68)
    set_target_properties(wxsparkle_ed25519 PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
if(MSVC)
    target_compile_options(wxsparkle_ed25519 PRIVATE /wd4244 /wd4146)
endif()

#---------------------------------------------------------------------------
# The library
#---------------------------------------------------------------------------

set(WXSPARKLE_SOURCES
    src/appcast.cpp
    src/appcontroller.cpp
    src/dll_api.cpp
    src/download.cpp
    src/error.cpp
    src/settings.cpp
    src/signatureverifier.cpp
    src/threads.cpp
    src/ui.cpp
    src/updatechecker.cpp
    src/updatedownloader.cpp
    src/appcast.h
    src/appcontroller.h
    src/download.h
    src/error.h
    src/fs.h
    src/platform.h
    src/settings.h
    src/signatureverifier.h
    src/threads.h
    src/ui.h
    src/updatechecker.h
    src/updatedownloader.h
    src/utils.h
    "${_gen_dir}/wxsparkle_cacert.h"
    "${_gen_dir}/wxsparkle_icons.h"
    "${_gen_dir}/wxsparkle_translations.h")

if(WIN32)
    list(APPEND WXSPARKLE_SOURCES src/settings_msw.cpp)
    if(BUILD_SHARED_LIBS)
        list(APPEND WXSPARKLE_SOURCES src/dllmain.cpp src/wxsparkle.rc)
    endif()
else()
    list(APPEND WXSPARKLE_SOURCES src/settings_unix.cpp)
endif()
if(APPLE)
    list(APPEND WXSPARKLE_SOURCES src/platform_mac.mm)
endif()
if(WXSPARKLE_RETRO68)
    list(APPEND WXSPARKLE_SOURCES src/platform_classicmac.cpp)
endif()

set(WXSPARKLE_PUBLIC_HEADERS
    include/winsparkle.h
    include/winsparkle-version.h)

add_library(wxsparkle
    ${WXSPARKLE_SOURCES}
    ${WXSPARKLE_PUBLIC_HEADERS}
    $<TARGET_OBJECTS:wxsparkle_ed25519>)
add_library(wxSparkle::wxsparkle ALIAS wxsparkle)

target_include_directories(wxsparkle
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
        "${_gen_dir}"
        3rdparty/ed25519/src)

target_compile_definitions(wxsparkle
    PRIVATE
        BUILDING_WIN_SPARKLE
        WXSPARKLE_USE_WEBVIEW=$<BOOL:${WXSPARKLE_HAVE_WEBVIEW}>
        WXSPARKLE_HAS_OPENSSL=$<BOOL:${WXSPARKLE_HAVE_OPENSSL}>
        $<$<BOOL:${WXSPARKLE_TRUST_NATIVE_CA}>:WXSPARKLE_TRUST_NATIVE_CA>)

if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(wxsparkle PUBLIC WIN_SPARKLE_STATIC)
endif()

if(WIN32)
    target_compile_definitions(wxsparkle PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN NOMINMAX
        WINVER=0x0601 _WIN32_WINNT=0x0601)
endif()

if(MSVC)
    target_compile_options(wxsparkle PRIVATE /W3 /utf-8 /bigobj)
else()
    target_compile_options(wxsparkle PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

set_target_properties(wxsparkle PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    OBJCXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE $<NOT:$<BOOL:${WXSPARKLE_RETRO68}>>
    PUBLIC_HEADER "${WXSPARKLE_PUBLIC_HEADERS}")

target_link_libraries(wxsparkle
    PRIVATE
        ${_wx_libs}
        ${_curl_lib}
        EXPAT::EXPAT)

if(WXSPARKLE_HAVE_OPENSSL)
    target_link_libraries(wxsparkle PRIVATE OpenSSL::Crypto)
endif()

if(WXSPARKLE_RETRO68)
    # <sys/socket.h>, for letting other threads run while waiting for the
    # network (WXSPARKLE_MACTCP_TARGET brings its own through wxsparkle_curl)
    if(NOT WXSPARKLE_MACTCP_TARGET)
        target_include_directories(wxsparkle PRIVATE "${WXSPARKLE_MACTCP_INCLUDE_DIR}")
    endif()
    if(BUILD_SHARED_LIBS)
        message(FATAL_ERROR "wxSparkle can only be built as a static library for classic Mac OS")
    endif()
endif()

# For macOS deployment targets before 10.15, which have no std::filesystem
# (src/fs.h): a parent project's ghc::filesystem target.
if(TARGET ghc-filesystem)
    target_link_libraries(wxsparkle PRIVATE ghc-filesystem)
endif()

find_package(Threads REQUIRED)
target_link_libraries(wxsparkle PRIVATE Threads::Threads)

# Only export the public API from a shared library, not symbols of statically
# linked dependencies (wxWidgets, OpenSSL, ...), which could clash with the
# host application's copies. (On Windows, only dllexport-ed symbols are.)
if(BUILD_SHARED_LIBS)
    if(APPLE)
        target_link_options(wxsparkle PRIVATE "LINKER:-exported_symbol,_win_sparkle_*")
    elseif(UNIX)
        target_link_options(wxsparkle PRIVATE
            "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/wxsparkle.map")
        set_property(TARGET wxsparkle APPEND PROPERTY LINK_DEPENDS
                     "${CMAKE_CURRENT_SOURCE_DIR}/cmake/wxsparkle.map")
    endif()
endif()

if(WIN32)
    target_link_libraries(wxsparkle PRIVATE version shell32 advapi32 comctl32)
elseif(APPLE)
    target_link_libraries(wxsparkle PRIVATE "-framework AppKit" "-framework CoreFoundation")
endif()

#---------------------------------------------------------------------------
# Tool, example, tests
#---------------------------------------------------------------------------

if(WXSPARKLE_BUILD_TOOL)
    add_executable(winsparkle-tool tools/winsparkle-tool.cpp $<TARGET_OBJECTS:wxsparkle_ed25519>)
    target_include_directories(winsparkle-tool PRIVATE
        include src 3rdparty/ed25519/src 3rdparty/argparse/include)
    if(WIN32)
        target_compile_definitions(winsparkle-tool PRIVATE UNICODE _UNICODE NOMINMAX)
    endif()
endif()

if(WXSPARKLE_BUILD_EXAMPLE AND WXSPARKLE_RETRO68)
    # A classic Mac OS application: wxsparkle-example.bin (MacBinary),
    # wxsparkle-example.dsk (HFS disk image) and wxsparkle-example.APPL.
    # (add_application() comes from Retro68's toolchain file.)
    #
    # Memory partition sizes in MB, see example.r.in
    if(CMAKE_SYSTEM_NAME STREQUAL "RetroPPC")
        set(WXSPARKLE_EXAMPLE_PREFERRED_SIZE 32)
        set(WXSPARKLE_EXAMPLE_MINIMUM_SIZE 26)
    else()
        set(WXSPARKLE_EXAMPLE_PREFERRED_SIZE 24)
        set(WXSPARKLE_EXAMPLE_MINIMUM_SIZE 18)
    endif()
    configure_file(examples/wx/retro68/example.r.in
                   "${CMAKE_CURRENT_BINARY_DIR}/wxsparkle-example.r" @ONLY)
    add_application(wxsparkle-example
        examples/wx/example.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/wxsparkle-example.r"
        CREATOR WXSP)
    target_link_libraries(wxsparkle-example PRIVATE wxsparkle ${_wx_libs})
    target_link_options(wxsparkle-example PRIVATE -Wl,--gc-sections)

    # Replace the disk image written by Rez, which is too small for the
    # application (see cmake/retro68_dsk.cmake)
    add_custom_command(TARGET wxsparkle-example_APPL POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                "-DBIN=${CMAKE_CURRENT_BINARY_DIR}/wxsparkle-example.bin"
                "-DDSK=${CMAKE_CURRENT_BINARY_DIR}/wxsparkle-example.dsk"
                -DNAME=wxsparkle-example
                "-DHFSUTILS=${RETRO68_ROOT}/bin"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/retro68_dsk.cmake"
        COMMENT "Creating wxsparkle-example.dsk"
        VERBATIM)
elseif(WXSPARKLE_BUILD_EXAMPLE)
    add_executable(wxsparkle-example WIN32 MACOSX_BUNDLE examples/wx/example.cpp)
    target_link_libraries(wxsparkle-example PRIVATE wxsparkle ${_wx_libs})
    set_target_properties(wxsparkle-example PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/examples/wx/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "wxSparkle Example"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.winsparkle.wxsparkle-example"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0")
    if(WIN32)
        target_sources(wxsparkle-example PRIVATE examples/wx/example.rc)
    endif()
endif()

if(WXSPARKLE_BUILD_TESTS)
    enable_testing()
    add_executable(wxsparkle-tests tests/tests.cpp)
    target_include_directories(wxsparkle-tests PRIVATE src "${_gen_dir}" 3rdparty/ed25519/src)
    target_compile_definitions(wxsparkle-tests PRIVATE
        WXSPARKLE_HAS_OPENSSL=$<BOOL:${WXSPARKLE_HAVE_OPENSSL}>)
    target_link_libraries(wxsparkle-tests PRIVATE wxsparkle ${_wx_libs} ${_curl_lib} EXPAT::EXPAT)
    if(WXSPARKLE_HAVE_OPENSSL)
        target_link_libraries(wxsparkle-tests PRIVATE OpenSSL::Crypto)
    endif()
    if(BUILD_SHARED_LIBS)
        # the tests use internal (non-exported) symbols
        message(STATUS "wxSparkle: unit tests require a static build, skipping")
        set_target_properties(wxsparkle-tests PROPERTIES EXCLUDE_FROM_ALL ON)
    else()
        add_test(NAME wxsparkle-tests COMMAND wxsparkle-tests)
        # set WXSPARKLE_TEST_NETWORK=1 in the environment to also run tests
        # that need internet access
    endif()
endif()

#---------------------------------------------------------------------------
# Installation
#---------------------------------------------------------------------------

if(WXSPARKLE_INSTALL)

if(WXSPARKLE_RETRO68 AND NOT DEFINED CMAKE_INSTALL_LIBDIR)
    # (GNUInstallDirs can't determine it, as the pointer size isn't known)
    set(CMAKE_INSTALL_LIBDIR lib)
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(WXSPARKLE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/wxSparkle")

install(TARGETS wxsparkle EXPORT wxSparkleTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(WXSPARKLE_BUILD_TOOL)
    install(TARGETS winsparkle-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(NOT BUILD_SHARED_LIBS)
    install(TARGETS wxsparkle_curl EXPORT wxSparkleTargets)
    if(TARGET wxsparkle_wx)
        install(TARGETS wxsparkle_wx EXPORT wxSparkleTargets)
    endif()
endif()

install(EXPORT wxSparkleTargets
        NAMESPACE wxSparkle::
        DESTINATION "${WXSPARKLE_CONFIG_INSTALL_DIR}")

configure_package_config_file(
    cmake/wxSparkleConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/wxSparkleConfig.cmake"
    INSTALL_DESTINATION "${WXSPARKLE_CONFIG_INSTALL_DIR}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/wxSparkleConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/wxSparkleConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/wxSparkleConfigVersion.cmake"
        DESTINATION "${WXSPARKLE_CONFIG_INSTALL_DIR}")

endif() # WXSPARKLE_INSTALL
