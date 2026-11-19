# Fails the build when the RetroPPC linker has put @FIX stubs in an XCOFF.
#
# A PowerPC branch reaches +-32 MB. When a call is out of reach -- in a big
# application, a call from code near the start of .text to the Toolbox import
# glue, which the linker always puts at the very end -- Retro68's ld calls
# through an @FIX csect of its own instead, and builds it against the wrong
# TOC offset: the call jumps through a garbage transition vector (SheepShaver:
# "EMUL_OP called with bogus selector", or a runaway PC). Calling the Toolbox
# function through a volatile function pointer (Toolbox() in
# src/wx/audio/internal/soundmanager.cpp, TOOLBOX() in
# src/wx/retro68/malloc.c) avoids the glue; this names the culprits.
#
#   cmake -DNM=<nm> -DOBJDUMP=<objdump> -DXCOFF=<file> -P Retro68CheckFixStubs.cmake

execute_process(
    COMMAND "${NM}" "${XCOFF}"
    OUTPUT_VARIABLE symbols
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${NM} ${XCOFF} failed")
endif()

if(NOT symbols MATCHES "@FIX")
    return()
endif()

message(FATAL_ERROR
    "${XCOFF} has @FIX stubs: the PowerPC linker's calls for out-of-reach "
    "branches (Toolbox import glue more than 32 MB away), which it gets wrong, "
    "so they crash at run time. Call the Toolbox functions in these through "
    "a volatile function pointer (see Toolbox() in "
    "src/wx/audio/internal/soundmanager.cpp). Find them with:\n"
    "  ${OBJDUMP} -d ${XCOFF} | grep -B30 '<@FIX' | grep '>:$'")
