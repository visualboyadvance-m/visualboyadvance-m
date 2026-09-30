# Fail the build when an XP-targeted binary imports an API Windows XP has not
# got.
#
# This is not a warning about something that might misbehave later. A static
# import of a missing function stops the binary loading at all -- the loader
# refuses the whole program before main() -- so the symptom is the emulator
# dying on startup with nothing to say. libusb arrived through SDL3's libusb
# feature and imported CancelIoEx and three condition variable calls that way,
# and it took a disassembly of the import table to find out why.
#
# Run as a post-build step; see where it is added in src/wx/CMakeLists.txt.
#
# Inputs:
#   EXE        the binary to check
#   FORBIDDEN  the name list, see cmake/xp-forbidden-imports.txt
#   OBJDUMP    the toolchain's objdump

if(NOT EXE OR NOT EXISTS "${EXE}")
    message(FATAL_ERROR "CheckXPImports: no binary to check at '${EXE}'")
endif()

if(NOT FORBIDDEN OR NOT EXISTS "${FORBIDDEN}")
    message(FATAL_ERROR "CheckXPImports: no name list at '${FORBIDDEN}'")
endif()

# A gate that quietly passes when it cannot run is worse than no gate, because
# it is trusted. Say so instead.
if(NOT OBJDUMP)
    message(FATAL_ERROR
        "CheckXPImports: no objdump, so the imports of ${EXE} cannot be checked")
endif()

execute_process(
    COMMAND "${OBJDUMP}" -p "${EXE}"
    OUTPUT_VARIABLE dump
    ERROR_VARIABLE  dump_err
    RESULT_VARIABLE dump_rc
)

if(NOT dump_rc EQUAL 0)
    message(FATAL_ERROR "CheckXPImports: objdump failed on ${EXE}: ${dump_err}")
endif()

file(STRINGS "${FORBIDDEN}" forbidden)

# The import table rows are "<vma> <ordinal> <hint> <name>", one per line and
# each starting with a tab. Take the trailing name off each; rows that are
# something else yield a word that is in no list and fall out below.
string(REGEX MATCHALL "\t[0-9a-f]+[^\n]*" rows "${dump}")

set(violations "")

foreach(row IN LISTS rows)
    string(REGEX MATCH "[A-Za-z_][A-Za-z0-9_]*$" name "${row}")

    if(name AND name IN_LIST forbidden)
        list(APPEND violations "${name}")
    endif()
endforeach()

if(violations)
    list(REMOVE_DUPLICATES violations)
    list(SORT violations)
    string(REPLACE ";" "\n    " violations_text "${violations}")

    message(FATAL_ERROR
        "${EXE} imports Win32 APIs that Windows XP does not have:\n"
        "    ${violations_text}\n\n"
        "A static import of a missing function stops the binary loading at all, so "
        "this build would not start on XP.\n\n"
        "These come from a library, not from this source tree: nothing here can "
        "call them, because the XP build compiles at _WIN32_WINNT=0x0501 where they "
        "are not even declared. Find the library with\n\n"
        "    nm <vcpkg>/installed/x86-mingw-static/lib/*.a | grep <name>\n\n"
        "and drop the feature that pulls it in, the way sdl3[libusb] was dropped.")
endif()
