
function(cygpath var path)
    execute_process(
        COMMAND cygpath -m ${path}
        OUTPUT_VARIABLE cyg_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    set(${var} ${cyg_path} PARENT_SCOPE)
endfunction()

function(check_clean_exit var)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE exit_status
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(NOT ${exit_status} EQUAL 0)
        # special case for msys2, where programs might complain about
        # not being win32 programs
        unset(cmd_str)
        foreach(param IN LISTS ARGN)
            set(cmd_str "${cmd_str} ${param}")
        endforeach()
        string(STRIP "${cmd_str}" cmd_str)

        execute_process(
            COMMAND sh -c "${cmd_str}"
            RESULT_VARIABLE exit_status
            OUTPUT_QUIET
            ERROR_QUIET
        )
    endif()

    set(${var} ${exit_status} PARENT_SCOPE)
endfunction()

function(find_wx_util var util)
    if(WIN32 OR EXISTS /etc/gentoo-release)
        # On win32, including cross builds we prefer the plain utility
        # name first from PATH, with the exception of -static for static
        # builds.
        #
        # On Gentoo /usr/bin/wx-config loads the eselected build, so we
        # want to try that first.
        #
        # This makes a one element of empty string list.

        if(VBAM_STATIC)
            set(conf_suffixes "static;")
        else()
            set(conf_suffixes  ";")
        endif()

        set(major_versions ";")
    endif()

    list(APPEND conf_suffixes  gtk4u gtk4 gtk3u gtk3 gtk2u gtk2 "")
    list(APPEND major_versions 4 3 2 "")

    foreach(conf_suffix IN LISTS conf_suffixes)
        foreach(major_version IN LISTS major_versions)
            foreach(minor_version RANGE 100 -1 -1)
                unset(suffix)
                if(conf_suffix)
                    set(suffix "-${conf_suffix}")
                endif()
                if(major_version)
                    set(suffix "${suffix}-${major_version}")

                    if(NOT minor_version EQUAL -1)
                        set(suffix "${suffix}.${minor_version}")
                    endif()
                endif()

                # find_program caches the result
                set(exe NOTFOUND CACHE INTERNAL "" FORCE)
                find_program(exe NAMES "${util}${suffix}")

                # try infix variant, as on FreeBSD
                if(NOT EXISTS ${exe})
                    string(REGEX REPLACE "^-" "" suffix "${suffix}")

                    string(REGEX REPLACE "-" "${suffix}-" try ${util})

                    set(exe NOTFOUND CACHE INTERNAL "" FORCE)
                    find_program(exe NAMES ${try})
                endif()

                if(EXISTS ${exe})
                    # check that the utility can be executed cleanly
                    # in case we find e.g. the wrong architecture binary
                    # when cross-compiling
                    check_clean_exit(exit_status ${exe} --help)

                    if(exit_status EQUAL 0)
                        set(${var} ${exe} PARENT_SCOPE)
                        return()
                    endif()
                endif()

                # don't iterate over minor versions for empty major version
                if(major_version STREQUAL "")
                    break()
                endif()
            endforeach()
        endforeach()

        # default to util name if not found, so the error is more clear
        set(${var} ${util} PARENT_SCOPE)
    endforeach()
endfunction()
