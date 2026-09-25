function(host_compile src dst_cmd)
    if(CMAKE_CROSSCOMPILING)
        unset(link_flags)
        set(dst "${dst_cmd}")

        if(CMAKE_HOST_WIN32)
            if(CMAKE_COMPILER_IS_GNUCXX)
                set(link_flags -Wl,--subsystem,console)
            endif()
        endif()

        if(MSVC)
            set(msvc_compile_script ${CMAKE_SOURCE_DIR}/cmake/MSVC_x86_Host_Compile.cmake)

            add_custom_command(
                OUTPUT ${dst}
                DEPENDS ${src} ${msvc_compile_script}
                COMMAND ${CMAKE_COMMAND} -D "src=${src}" -D "dst=${dst}" -P ${msvc_compile_script}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        else()
            set(dst ${dst_cmd})

            # `cc` is only a safe bet on a Unix host, and MSVC above catches a
            # Windows host only when the *target* toolchain is MSVC too. A
            # Windows host cross-compiling with anything else -- the Android
            # NDK's clang, say -- reaches here and has no `cc`, so go looking
            # for a compiler that is actually installed. The target toolchain
            # is no use for this: it emits binaries for the target.
            if(NOT VBAM_HOST_CC)
                if(APPLE AND EXISTS /usr/bin/clang)
                    set(VBAM_HOST_CC /usr/bin/clang CACHE FILEPATH
                        "C compiler that builds executables for the build host")
                else()
                    if(CMAKE_HOST_WIN32)
                        # gcc before the MSVC drivers: a MinGW/MSYS2 compiler
                        # needs no environment set up around it, while clang
                        # and cl want the INCLUDE/LIB a VS shell exports.
                        set(host_cc_names cc gcc clang cl)
                    else()
                        set(host_cc_names cc clang gcc)
                    endif()

                    # Restricted to this machine's PATH on purpose. The
                    # cross toolchain puts its own bin directory into CMake's
                    # program search -- the NDK's does -- and a plain
                    # find_program() picks that clang up and quietly builds
                    # for the target.
                    find_program(VBAM_HOST_CC NAMES ${host_cc_names}
                        PATHS ENV PATH NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH
                        DOC "C compiler that builds executables for the build host")
                endif()
            endif()

            if(NOT VBAM_HOST_CC)
                message(FATAL_ERROR
                    "No C compiler found that can build ${src} for this machine. "
                    "One is needed to build a tool the cross build runs during "
                    "the build. Pass -DVBAM_HOST_CC=<path to a host cc>.")
            endif()

            # Guard against picking the cross toolchain back up: an NDK bin
            # directory on PATH would hand us a compiler for the target.
            get_filename_component(host_cc_dir   "${VBAM_HOST_CC}"     DIRECTORY)
            get_filename_component(target_cc_dir "${CMAKE_C_COMPILER}" DIRECTORY)

            if(host_cc_dir STREQUAL target_cc_dir)
                message(FATAL_ERROR
                    "The only C compiler found for this machine, ${VBAM_HOST_CC}, is the "
                    "cross toolchain's own and builds for the target. "
                    "Pass -DVBAM_HOST_CC=<path to a host cc>.")
            endif()

            get_filename_component(host_cc_name "${VBAM_HOST_CC}" NAME_WE)

            if(host_cc_name STREQUAL "cl")
                # The MSVC driver spells its output option differently, and
                # names the object after the source in the working directory
                # unless told otherwise, where tools built from sources of the
                # same name would collide (see MSVC_x86_Host_Compile.cmake).
                add_custom_command(
                    OUTPUT ${dst}
                    DEPENDS ${src}
                    COMMAND ${VBAM_HOST_CC} /nologo ${src} /Fo:${dst}.obj /Fe:${dst}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            else()
                add_custom_command(
                    OUTPUT ${dst}
                    DEPENDS ${src}
                    COMMAND ${VBAM_HOST_CC} ${src} -o ${dst} ${link_flags}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                )
            endif()
        endif()
    else()
        # NAME_WE: dst_cmd may carry the host executable suffix, which belongs
        # to the file add_executable() produces, not to the target name.
        get_filename_component(dst ${dst_cmd} NAME_WE)

        add_executable(${dst} ${src})

        # this is necessary because we override main with SDL_main
        target_compile_definitions(${dst} PRIVATE -Dmain=main)
    endif()
endfunction()
