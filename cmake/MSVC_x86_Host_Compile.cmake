# This is a hack to be run with cmake -P to compile a source C program passed
# in `src` to the `dst` executable using the host MSVC x86 toolchain.

find_program(cl_path NAME cl.exe HINTS ENV PATH)

string(REGEX REPLACE "[^/]+/cl\\.exe$" "x86/cl.exe" cl_path "${cl_path}")

set(orig_lib "$ENV{LIB}")
set(new_lib)

foreach(lib $ENV{LIB})
    string(REGEX REPLACE "[^\\]+$" "x86" lib "${lib}")

    list(APPEND new_lib "${lib}")
endforeach()

set(ENV{LIB} "${new_lib}")

execute_process(
    COMMAND ${cl_path} /nologo ${src} /Fe:${dst}
    OUTPUT_QUIET
)

set(ENV{LIB} "${orig_lib}")
