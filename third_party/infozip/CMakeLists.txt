# Info-ZIP `zip` 3.0 (released 2008-07-05), the subset of the upstream
# distribution that builds the `zip` program itself.
#
# The build needs a zip compressor for the translations archive, the release
# archives and wxrc, and not every machine has one: the GitHub Windows 11 Arm
# image has no zip and no Windows PowerShell, and installing one from a
# package feed makes every build depend on that feed being up. Building it
# here instead means a checkout is all that is ever required.
#
# Dropped from upstream: the zipcloak/zipnote/zipsplit utilities, the DLL and
# API wrappers, the assembler CRC and match routines (NO_ASM below), bzip2 and
# zlib support (zip has its own deflate), and every port but win32 and unix.
# The sources that remain are unmodified; see LICENSE for the terms.

# VBA-M's own definitions and warning flags are inherited from the parent
# directory, and they do not belong on this code. UNICODE in particular is
# fatal here: the MSVC toolchain file defines it for VBA-M, and it would
# silently point Info-ZIP's plain CreateFile/FindFirstFile calls at the wide
# API they were not written against.
set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS "")
set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "")

set(INFOZIP_SOURCES
    crc32.c
    crypt.c
    deflate.c
    fileio.c
    globals.c
    trees.c
    ttyio.c
    util.c
    zip.c
    zipfile.c
    zipup.c
)

if(WIN32)
    list(APPEND INFOZIP_SOURCES
        win32/nt.c
        win32/win32.c
        win32/win32i64.c
        win32/win32zip.c
    )
else()
    list(APPEND INFOZIP_SOURCES unix/unix.c)
endif()

add_executable(infozip-zip ${INFOZIP_SOURCES})

# The quoted includes are written relative to this directory, as
# "win32/zipup.h" and the like, so it is the one include path needed.
target_include_directories(infozip-zip PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# NO_ASM: use the C crc32 and longest_match rather than the x86 assembler
# ones, which is also the only thing that can work on Arm.
target_compile_definitions(infozip-zip PRIVATE NO_ASM)

if(WIN32)
    target_compile_definitions(infozip-zip PRIVATE
        WIN32
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
    )

    # win32.c reads the volume label and the file times, nt.c the security
    # descriptors.
    target_link_libraries(infozip-zip PRIVATE user32 advapi32)

    # zip.h defines CR as 13, and <winnt.h> has had a bitfield named CR in
    # IMAGE_ARM64_RUNTIME_FUNCTION_ENTRY since long after this code was
    # written, so the Windows headers no longer parse once zip.h has been
    # seen. Every source below includes <windows.h> itself, a few lines after
    # zip.h; pulling it in ahead of both only reorders what they already
    # include, and WIN32_LEAN_AND_MEAN is the empty definition each one makes
    # right before its own include.
    set(INFOZIP_WINDOWS_FIRST
        zip.c
        zipfile.c
        win32/nt.c
        win32/win32.c
        win32/win32i64.c
        win32/win32zip.c
    )

    if(MSVC)
        set(INFOZIP_FORCE_INCLUDE /FIwindows.h)
    else()
        set(INFOZIP_FORCE_INCLUDE -include windows.h)
    endif()

    target_compile_definitions(infozip-zip PRIVATE WIN32_LEAN_AND_MEAN=)

    set_source_files_properties(${INFOZIP_WINDOWS_FIRST}
        TARGET_DIRECTORY infozip-zip
        PROPERTIES COMPILE_OPTIONS "${INFOZIP_FORCE_INCLUDE}"
    )
else()
    include(CheckIncludeFile)
    check_include_file(dirent.h  HAVE_DIRENT_H)
    check_include_file(termios.h HAVE_TERMIOS_H)

    # LARGE_FILE_SUPPORT turns on the LFS feature macros in unix/osdep.h and,
    # with them, Zip64; UIDGID_NOT_16BIT stores the 32-bit ids every system
    # this builds on actually has.
    target_compile_definitions(infozip-zip PRIVATE
        UNIX
        LARGE_FILE_SUPPORT
        UIDGID_NOT_16BIT
    )

    if(HAVE_DIRENT_H)
        target_compile_definitions(infozip-zip PRIVATE HAVE_DIRENT_H)
    endif()

    if(HAVE_TERMIOS_H)
        target_compile_definitions(infozip-zip PRIVATE HAVE_TERMIOS_H)
    endif()
endif()

# 2008 C, compiled by VBA-M's warning flags, is a page of noise that no one
# here is going to act on, and -Werror would stop the build outright.
if(MSVC)
    target_compile_options(infozip-zip PRIVATE /w)
else()
    target_compile_options(infozip-zip PRIVATE -w -Wno-error)
endif()

# The parent sets ZIP_PROGRAM to this path, so it has to be exact: name the
# per-configuration directories too, or a multi-config generator would put the
# program in a Debug/ or Release/ subdirectory of its own.
set_target_properties(infozip-zip PROPERTIES
    OUTPUT_NAME              zip
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config_upper)
    set_target_properties(infozip-zip PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}
    )
endforeach()
