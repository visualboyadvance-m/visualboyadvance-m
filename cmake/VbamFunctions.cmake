
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

    set(names "${util}${suffix}")

    # infix variant, as on FreeBSD
    string(REGEX REPLACE "^-" "" infix "${suffix}")
    string(REGEX REPLACE "-" "${infix}-" infix_name "${util}")

    if(NOT infix_name STREQUAL "${util}${suffix}")
        list(APPEND names "${infix_name}")
    endif()

    # Walk PATH a directory at a time instead of taking find_program's first
    # hit anywhere on it.  A cross build has an unusable target-architecture
    # copy of the utility in the build root, which comes first on PATH and
    # would otherwise hide a usable one further along.  find_program is still
    # what looks inside each directory, so CMAKE_EXECUTABLE_SUFFIX is honored.
    file(TO_CMAKE_PATH "$ENV{PATH}" path_dirs)

    foreach(dir IN LISTS path_dirs)
        # find_program caches the result
        set(exe NOTFOUND CACHE INTERNAL "" FORCE)
        find_program(exe NAMES ${names} PATHS "${dir}" NO_DEFAULT_PATH)

        # Copy it out of the cache entry, which stays visible to our caller.
        set(candidate "${exe}")
        set(exe NOTFOUND CACHE INTERNAL "" FORCE)

        if(EXISTS "${candidate}")
            # check that the utility can be executed cleanly
            # in case we find e.g. the wrong architecture binary
            # when cross-compiling
            check_clean_exit(exit_status "${candidate}" --help)

            if(exit_status EQUAL 0)
                set("${var}" "${candidate}" PARENT_SCOPE)
                return()
            endif()
        endif()
    endforeach()

    # Nothing usable on PATH.  Fall back to CMake's own search, which consults
    # CMAKE_PROGRAM_PATH -- where vcpkg registers each port's tools directory,
    # and so the only place a vcpkg-installed utility appears at all.  The walk
    # above stays first: its NO_DEFAULT_PATH is what stops a cross build's
    # target-architecture copy in the build root from winning, and the same
    # check below still rejects one that cannot run here.
    set(exe NOTFOUND CACHE INTERNAL "" FORCE)
    find_program(exe NAMES ${names})

    set(candidate "${exe}")
    set(exe NOTFOUND CACHE INTERNAL "" FORCE)

    if(EXISTS "${candidate}")
        check_clean_exit(exit_status "${candidate}" --help)

        if(exit_status EQUAL 0)
            set("${var}" "${candidate}" PARENT_SCOPE)
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

    # wx::base only exists when wxWidgets was found through its own CMake
    # config package.  A wx-config based find (the classic FindwxWidgets
    # module, which is what wxWidgets_CONFIG_EXECUTABLE selects) defines no
    # imported targets, so fall back to the library list it does set.
    if(TARGET wx::base)
        get_target_property(wx_base_lib_prop wx::base LOCATION)
    else()
        string(REPLACE ";" " " wx_base_lib_prop "${wxWidgets_LIBRARIES}")
    endif()

    string(STRIP "${wx_base_lib_prop}" wx_base_lib)

    if(wx_base_lib MATCHES "wx_baseu?-([0-9]+)\\.([0-9]+)\\.")
        set(lib_major "${CMAKE_MATCH_1}")
        set(lib_minor "${CMAKE_MATCH_2}")
    endif()

    foreach(conf_suffix IN LISTS conf_suffixes)
        if(lib_major AND lib_minor)
            unset(wx_util_exe)
            try_wx_util(wx_util_exe "${util}" "${conf_suffix}" "${lib_major}" "${lib_minor}")

            if(wx_util_exe)
                set("${var}" "${wx_util_exe}" PARENT_SCOPE)
                return()
            endif()
        endif()

        foreach(major_version IN LISTS major_versions)
            foreach(minor_version RANGE 30 -1 -1)
                unset(wx_util_exe)
                try_wx_util(wx_util_exe "${util}" "${conf_suffix}" "${major_version}" "${minor_version}")

                if(wx_util_exe)
                    set("${var}" "${wx_util_exe}" PARENT_SCOPE)
                    return()
                endif()

                # don't iterate over minor versions for empty major version
                if(major_version STREQUAL "")
                    break()
                endif()
            endforeach()
        endforeach()

    endforeach()

    # Leave ${var} unset when nothing was found, so the caller's check fails at
    # configure time.  Defaulting to the bare utility name reads as friendlier
    # but passes that check, and the name then reaches add_custom_command as a
    # command that is not on PATH: the build died 146 targets in on "no such
    # file or directory", naming neither wxrc nor the search that missed it.
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
