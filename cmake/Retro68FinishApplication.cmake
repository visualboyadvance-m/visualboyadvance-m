# Finishes a Retro68 application that Rez has written as MacBinary (cmake -P):
#
#   - writes OUT_BIN, the MacBinary with the application's bundle bit set
#     (SET_FLAGS, tools/retro68/macbinary-set-flags.c): without it the Finder
#     ignores the application's BNDL resource and shows a generic icon
#     instead of its own;
#   - makes OUT_DSK, an HFS disk image holding that, with hfsutils;
#   - sets the bundle bit of the native APPL copy too, on a Mac host.
#
# Rez can write a disk image itself (--cc <name>.dsk), but it sizes the image
# for the file so tightly that a large application (the 24 MB PowerPC one)
# ends up on it without its resource fork and with its data fork cut short.
#
# Variables: INPUT (Rez's MacBinary), NAME (the file's name on the disk),
# OUT_BIN, OUT_DSK, SET_FLAGS, HFSUTILS (the directory with hformat, hmount,
# ...), WORK_DIR (for hfsutils' state), and optionally APPL, TYPE and CREATOR
# for the native copy.

foreach(var INPUT NAME OUT_BIN OUT_DSK SET_FLAGS HFSUTILS WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "Retro68FinishApplication.cmake: ${var} is not set")
    endif()
endforeach()

# hfsutils keep the mounted volume in $HOME/.hcwd; a home of our own keeps two
# builds running at once (68k and PowerPC) off each other's volume.
set(hfs_home "${WORK_DIR}/hfsutils-home")
file(MAKE_DIRECTORY "${hfs_home}")

function(hfs)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env "HOME=${hfs_home}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    if(NOT result EQUAL 0)
        list(JOIN ARGN " " command)
        message(FATAL_ERROR "${command} failed (${result}):\n${output}")
    endif()
endfunction()

# The application plus a quarter for the volume's own structures and
# allocation rounding, and at least an 800K floppy's worth.
# 0x2000 is hasBundle.
file(REMOVE "${OUT_BIN}")
execute_process(
    COMMAND "${SET_FLAGS}" "${INPUT}" "${OUT_BIN}" 2000
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${SET_FLAGS} could not set the Finder flags of ${INPUT}")
endif()

file(SIZE "${OUT_BIN}" app_size)
math(EXPR image_kb "(${app_size} + ${app_size} / 4) / 1024 + 512")
if(image_kb LESS 800)
    set(image_kb 800)
endif()

file(REMOVE "${OUT_DSK}")
execute_process(
    COMMAND dd if=/dev/zero "of=${OUT_DSK}" bs=1024 "count=${image_kb}"
    RESULT_VARIABLE result
    OUTPUT_QUIET ERROR_QUIET
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "dd could not make ${OUT_DSK}")
endif()

hfs("${HFSUTILS}/hformat" -l "${NAME}" "${OUT_DSK}")
hfs("${HFSUTILS}/hmount" "${OUT_DSK}")
hfs("${HFSUTILS}/hcopy" -m "${OUT_BIN}" ":${NAME}")
hfs("${HFSUTILS}/humount")

# The native copy keeps its Finder information in an extended attribute on a
# Mac: type, creator, then the Finder flags, where 0x2000 is hasBundle.
if(APPL AND CMAKE_HOST_APPLE AND EXISTS "${APPL}")
    find_program(XATTR xattr)
    if(XATTR)
        set(finder_info "")
        foreach(code "${TYPE}" "${CREATOR}")
            string(HEX "${code}" code_hex)
            string(APPEND finder_info "${code_hex}")
        endforeach()
        string(APPEND finder_info "2000")
        string(LENGTH "${finder_info}" len)
        while(len LESS 64)
            string(APPEND finder_info "00")
            string(LENGTH "${finder_info}" len)
        endwhile()
        execute_process(COMMAND "${XATTR}" -wx com.apple.FinderInfo "${finder_info}" "${APPL}")
    endif()
endif()
