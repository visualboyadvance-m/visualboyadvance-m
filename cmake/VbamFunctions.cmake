
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

function(try_wx_util var util conf_suffix major_version minor_version)
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
    if(NOT EXISTS "${exe}")
        string(REGEX REPLACE "^-" "" suffix "${suffix}")

        string(REGEX REPLACE "-" "${suffix}-" try "${util}")

        set(exe NOTFOUND CACHE INTERNAL "" FORCE)
        find_program(exe NAMES "${try}")
    endif()

    if(EXISTS "${exe}")
        # check that the utility can be executed cleanly
        # in case we find e.g. the wrong architecture binary
        # when cross-compiling
        check_clean_exit(exit_status "${exe}" --help)

        if(exit_status EQUAL 0)
            set("${var}" "${exe}" PARENT_SCOPE)
            return()
        endif()
    endif()
endfunction()

function(find_wx_util var util)
    if((WIN32 AND (NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")) OR EXISTS /etc/gentoo-release)
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

    list(APPEND conf_suffixes  "" gtk3u gtk3 gtk2u gtk2)
    list(APPEND major_versions "" 3)

    get_target_property(wx_base_lib_prop wx::base LOCATION)
    string(STRIP "${wx_base_lib_prop}" wx_base_lib)

    if(wx_base_lib MATCHES "wx_baseu?-([0-9]+)\\.([0-9]+)\\.")
        set(lib_major "${CMAKE_MATCH_1}")
        set(lib_minor "${CMAKE_MATCH_2}")
    endif()

    foreach(conf_suffix IN LISTS conf_suffixes)
        if(lib_major AND lib_minor)
            try_wx_util(exe "${util}" "${conf_suffix}" "${lib_major}" "${lib_minor}")

            if(exe)
                set("${var}" "${exe}" PARENT_SCOPE)
                return()
            endif()
        endif()

        foreach(major_version IN LISTS major_versions)
            foreach(minor_version RANGE 30 -1 -1)
                try_wx_util(exe "${util}" "${conf_suffix}" "${major_version}" "${minor_version}")

                if(exe)
                    set("${var}" "${exe}" PARENT_SCOPE)
                    return()
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

# Drop link items naming a file that is not there from the interfaces of the
# given imported targets, and of the targets those pull in.
#
# A vcpkg package records the absolute path of whatever it was linked against,
# the Android NDK's per-API-level stubs included, so a package built against one
# NDK asks a machine with another NDK for libraries it does not have:
# NanoSVGTargets.cmake carries
# <sdk>/ndk/<other>/toolchains/llvm/prebuilt/<host>/sysroot/usr/lib/<triple>/<api>/libm.so.
#
# CMake treats an absolute link item as a file dependency, so ninja refuses to
# build anything that inherits one, try_compile() included -- and the failure
# surfaces a long way from the cause. A missing libm took out the
# check_cxx_source_compiles() in Qt's FindGLESv2, which was reported as Qt6Gui,
# and then Qt6 itself, not being found. The clang driver links the sysroot
# libraries from the NDK actually in use, so dropping the recorded ones costs
# nothing.
#
# This is deliberately narrow: an absolute path to a library file that does not
# exist cannot be right, whatever it names.
function(vbam_drop_missing_link_items)
    get_property(visited GLOBAL PROPERTY VBAM_DROP_MISSING_VISITED)

    foreach(target ${ARGN})
        if(NOT TARGET "${target}")
            continue()
        endif()

        # Properties are read and written through the target an alias names.
        get_target_property(aliased "${target}" ALIASED_TARGET)
        if(aliased)
            set(target "${aliased}")
        endif()

        if(target IN_LIST visited)
            continue()
        endif()

        set_property(GLOBAL APPEND PROPERTY VBAM_DROP_MISSING_VISITED "${target}")
        list(APPEND visited "${target}")

        get_target_property(items "${target}" INTERFACE_LINK_LIBRARIES)

        if(NOT items)
            continue()
        endif()

        set(kept "")
        set(nested "")

        foreach(item IN LISTS items)
            if(item MATCHES "^/"
                    AND item MATCHES "\\.(a|so|dylib|lib)(\\.[0-9.]+)?$"
                    AND NOT EXISTS "${item}")
                message(STATUS "${target}: dropping missing link item ${item}")
                continue()
            endif()

            list(APPEND kept "${item}")

            if(TARGET "${item}")
                list(APPEND nested "${item}")
            endif()
        endforeach()

        set_target_properties("${target}" PROPERTIES INTERFACE_LINK_LIBRARIES "${kept}")

        if(nested)
            vbam_drop_missing_link_items(${nested})
        endif()
    endforeach()
endfunction()
