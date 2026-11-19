#
#  wxSparkle -- a wxWidgets port of WinSparkle (https://winsparkle.org)
#
#  Copyright (C) 2009-2026 Vaclav Slavik
#  SPDX-License-Identifier: MIT (see COPYING)
#

# (Re)creates the HFS disk image of a Retro68 application from its MacBinary
# file:
#
#   cmake -DBIN=<app.bin> -DDSK=<app.dsk> -DNAME=<volume/file name>
#         -DHFSUTILS=<dir with hformat, hmount, hcopy, humount>
#         -P retro68_dsk.cmake
#
# Rez can write the disk image itself (add_application() does it), but it
# sizes the volume too small for big applications such as wxWidgets ones: the
# application gets truncated and the Finder refuses to launch it with error
# -39 (eofErr).

foreach(_var BIN DSK NAME HFSUTILS)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "${_var} must be defined")
    endif()
endforeach()

# Room for the volume's structures, the Desktop database the Finder creates
# and what the application may write next to itself.
file(SIZE "${BIN}" _app_size)
math(EXPR _image_kb "(${_app_size} + ${_app_size} / 4) / 1024 + 512")

file(REMOVE "${DSK}")
execute_process(
    COMMAND dd if=/dev/zero "of=${DSK}" bs=1024 "count=${_image_kb}"
    RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
if(_rc)
    message(FATAL_ERROR "Could not create ${DSK}")
endif()

# hfsutils keep their current volume in $HOME/.hcwd: use a private one, so
# that concurrent builds don't interfere.
get_filename_component(_dsk_dir "${DSK}" DIRECTORY)
set(_home "${_dsk_dir}/.hfsutils-${NAME}")
file(MAKE_DIRECTORY "${_home}")
set(ENV{HOME} "${_home}")

function(_hfs)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE _rc OUTPUT_QUIET)
    if(_rc)
        string(REPLACE ";" " " _cmd "${ARGN}")
        message(FATAL_ERROR "Failed: ${_cmd}")
    endif()
endfunction()

_hfs("${HFSUTILS}/hformat" -l "${NAME}" "${DSK}")
_hfs("${HFSUTILS}/hmount" "${DSK}")
_hfs("${HFSUTILS}/hcopy" -m "${BIN}" ":${NAME}")
_hfs("${HFSUTILS}/humount")
file(REMOVE_RECURSE "${_home}")
