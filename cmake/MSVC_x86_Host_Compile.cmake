# This is a hack to be run with cmake -P to compile a source C program passed
# in `src` to the `dst` executable using the host MSVC x86 toolchain.
#
# The environment this runs in is set up for the *target*: an arm64 cross build
# points cl.exe and LIB at arm64. Both are aimed back at x86 here, that being
# the one Windows architecture a host of any of them can run.

find_program(cl_path NAMES cl.exe HINTS ENV PATH)

if(NOT cl_path)
    message(FATAL_ERROR
        "No cl.exe on PATH to build ${src} with; this has to run in an MSVC environment.")
endif()

# find_program() hands back a normalized path, so this matches on Windows.
string(REGEX REPLACE "[^/]+/cl\\.exe$" "x86/cl.exe" cl_x86_path "${cl_path}")

if(NOT EXISTS "${cl_x86_path}")
    message(FATAL_ERROR
        "No x86 cl.exe at ${cl_x86_path}, worked out from ${cl_path}. Building ${src} "
        "into something this machine can run needs the MSVC x86 build tools.")
endif()

set(orig_lib "$ENV{LIB}")
set(new_lib)

foreach(lib $ENV{LIB})
    string(REGEX REPLACE "[^\\]+$" "x86" lib "${lib}")

    list(APPEND new_lib "${lib}")
endforeach()

set(ENV{LIB} "${new_lib}")

# Output is kept rather than thrown away so that a failure can be reported with
# it. Discarding it along with the exit status left a failed compile silent, to
# be discovered later on as a missing ${dst} and a much less obvious error.
#
# The object file is named after ${dst}: cl.exe otherwise drops it in the
# working directory named after ${src}, and two tools built from sources of the
# same name at the same time (VBA-M's bin2c and the DLSS NR tree's) then fight
# over it and one fails with "Cannot open compiler generated file".
execute_process(
    COMMAND ${cl_x86_path} /nologo ${src} /Fo:${dst}.obj /Fe:${dst}
    OUTPUT_VARIABLE cl_output
    ERROR_VARIABLE  cl_output
    RESULT_VARIABLE cl_result
)

set(ENV{LIB} "${orig_lib}")

if(NOT cl_result EQUAL 0)
    message(FATAL_ERROR
        "Building ${src} with ${cl_x86_path} failed (${cl_result}):\n${cl_output}")
endif()

if(NOT EXISTS "${dst}")
    message(FATAL_ERROR
        "${cl_x86_path} reported success but produced no ${dst}:\n${cl_output}")
endif()
