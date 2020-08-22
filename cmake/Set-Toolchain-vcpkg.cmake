if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    return()
endif()

function(vcpkg_seconds)
    if(CMAKE_HOST_SYSTEM MATCHES Windows OR ((NOT DEFINED CMAKE_HOST_SYSTEM) AND WIN32))
        execute_process(
            COMMAND cmd /c echo %TIME:~0,8%
            OUTPUT_VARIABLE time
        )
    else()
        execute_process(
            COMMAND date +'%H:%M:%S'
            OUTPUT_VARIABLE time
        )
    endif()

    string(SUBSTRING "${time}" 0 2 hours)
    string(SUBSTRING "${time}" 3 2 minutes)
    string(SUBSTRING "${time}" 6 2 secs)

    math(EXPR seconds "(${hours} * 60 * 60) + (${minutes} * 60) + ${secs}")

    set(seconds ${seconds} PARENT_SCOPE)
endfunction()

function(vcpkg_check_git_status git_status)
    if(NOT git_status EQUAL 0)
        message(FATAL_ERROR "Error updating vcpkg from git, please make sure git for windows is installed correctly, it can be installed from Visual Studio components")
    endif()
endfunction()

function(vcpkg_get_first_upgrade vcpkg_exe)
    # First get the list of upgraded ports.
    execute_process(
        COMMAND ${vcpkg_exe} upgrade
        OUTPUT_VARIABLE upgradable
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY ${VCPKG_ROOT}
    )

    string(REGEX REPLACE "\r?\n" ";" upgrade_lines "${upgradable}")

    foreach(line ${upgrade_lines})
        if(line MATCHES "^  [* ] ")
            string(REGEX REPLACE "^  [* ] " "" pkg ${line})

            # Check if package is up-to-date, but would be rebuilt due to other dependencies.
            execute_process(
                COMMAND ${vcpkg_exe} upgrade ${pkg}
                OUTPUT_VARIABLE pkg_upgrade
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )

            if(NOT pkg_upgrade MATCHES up-to-date)
                # Prefer upgrading zlib before anything else.
                if(NOT first_upgrade OR pkg MATCHES zlib)
                    set(first_upgrade ${pkg})
                endif()
            endif()
        endif()
    endforeach()

    set(first_upgrade ${first_upgrade} PARENT_SCOPE)
endfunction()

function(vcpkg_deps_fixup vcpkg_exe)
    # Get installed list.
    execute_process(
        COMMAND ${vcpkg_exe} list
        OUTPUT_VARIABLE pkg_list
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY ${VCPKG_ROOT}
    )

    # If libvorbis is NOT installed but libogg is, remove libvorbis recursively.
    if(pkg_list MATCHES libogg AND (NOT pkg_list MATCHES libvorbis))
        execute_process(
            COMMAND "${vcpkg_exe}" remove --recurse libogg:${VCPKG_TARGET_TRIPLET}
            WORKING_DIRECTORY ${VCPKG_ROOT}
        )
    endif()
endfunction()

function(vcpkg_set_toolchain)
    if(NOT DEFINED ENV{VCPKG_ROOT})
        if(WIN32)
            if(DEFINED ENV{CI} OR EXISTS /vcpkg)
                set(VCPKG_ROOT /vcpkg)
            elseif(EXISTS c:/vcpkg)
                set(VCPKG_ROOT c:/vcpkg)
            endif()
        endif()

        if(NOT DEFINED VCPKG_ROOT)
            get_filename_component(VCPKG_ROOT ${CMAKE_SOURCE_DIR}/../vcpkg ABSOLUTE)
        endif()

        set(ENV{VCPKG_ROOT} ${VCPKG_ROOT})
    else()
        set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    endif()

    set(VCPKG_ROOT ${VCPKG_ROOT} CACHE FILEPATH "vcpkg installation root path" FORCE)

    if(NOT EXISTS ${VCPKG_ROOT})
        get_filename_component(root_parent ${VCPKG_ROOT}/.. ABSOLUTE)

        execute_process(
            COMMAND git clone https://github.com/microsoft/vcpkg.git
            RESULT_VARIABLE git_status
            WORKING_DIRECTORY ${root_parent}
        )

        vcpkg_check_git_status(${git_status})
    else()
        # this is the case when we cache vcpkg/installed with the appveyor build cache
        if(NOT EXISTS ${VCPKG_ROOT}/.git)
            set(git_commands
                "git init"
                "git remote add origin https://github.com/microsoft/vcpkg.git"
                "git fetch --all --prune"
                "git reset --hard origin/master"
                "git branch --set-upstream-to=origin/master master"
            )
            foreach(git_command ${git_commands})
                separate_arguments(git_command)

                execute_process(
                    COMMAND ${git_command}
                    RESULT_VARIABLE git_status
                    WORKING_DIRECTORY ${VCPKG_ROOT}
                )

                vcpkg_check_git_status(${git_status})
            endforeach()
        else()
            execute_process(
                COMMAND git fetch origin
                RESULT_VARIABLE git_status
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
            vcpkg_check_git_status(${git_status})

            execute_process(
                COMMAND git status
                RESULT_VARIABLE git_status
                OUTPUT_VARIABLE git_status_text
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
            vcpkg_check_git_status(${git_status})

            set(git_up_to_date FALSE)

            if(git_status_text MATCHES "Your branch is up to date with")
                set(git_up_to_date TRUE)
            endif()

            if(NOT git_up_to_date)
                execute_process(
                    COMMAND git pull --rebase
                    RESULT_VARIABLE git_status
                    WORKING_DIRECTORY ${VCPKG_ROOT}
                )

                vcpkg_check_git_status(${git_status})
            endif()
        endif()

        vcpkg_check_git_status(${git_status})
    endif()

    # build latest vcpkg, if needed
    if(NOT git_up_to_date)
        if(WIN32)
            execute_process(
                COMMAND bootstrap-vcpkg.bat
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        else()
            execute_process(
                COMMAND ./bootstrap-vcpkg.sh
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        endif()
    endif()

    foreach(pkg ${VCPKG_DEPS})
        list(APPEND VCPKG_DEPS_QUALIFIED ${pkg}:${VCPKG_TARGET_TRIPLET})
    endforeach()

    if(WIN32)
        set(vcpkg_exe vcpkg)
    else()
        set(vcpkg_exe ./vcpkg)
    endif()

    # update portfiles
    execute_process(
        COMMAND ${vcpkg_exe} update
        WORKING_DIRECTORY ${VCPKG_ROOT}
    )

    # Get number of seconds since midnight (might be wrong if am/pm is in effect on Windows.)
    vcpkg_seconds()
    set(began ${seconds})

    # Limit total installation time to 30 minutes to not overrun CI time limit.
    math(EXPR time_limit "${began} + (30 * 60)")

    vcpkg_deps_fixup("${vcpkg_exe}")

    # Install core deps.
    execute_process(
        COMMAND ${vcpkg_exe} install ${VCPKG_DEPS_QUALIFIED}
        WORKING_DIRECTORY ${VCPKG_ROOT}
    )

    # Install optional deps, within time limit.
    list(LENGTH VCPKG_DEPS_OPTIONAL optionals_list_len)
    math(EXPR optionals_list_last "${optionals_list_len} - 1")

    foreach(i RANGE 0 ${optionals_list_last} 2)
        list(GET VCPKG_DEPS_OPTIONAL ${i} dep)

        math(EXPR var_idx "${i} + 1")

        list(GET VCPKG_DEPS_OPTIONAL ${var_idx} var)
        set(val "${${var}}")

        vcpkg_seconds()

        if(seconds LESS time_limit AND (val OR val STREQUAL ""))
            set(dep_qualified "${dep}:${VCPKG_TARGET_TRIPLET}")

            execute_process(
                COMMAND ${vcpkg_exe} install ${dep_qualified}
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )

            set(${var} ON)
        else()
            set(${var} OFF)
        endif()
    endforeach()

    # If ports have been updated, and there is time, rebuild cache one at a time to not overrun the CI time limit.
    vcpkg_seconds()

    if(seconds LESS time_limit)
        vcpkg_get_first_upgrade(${vcpkg_exe})

        if(DEFINED first_upgrade)
            execute_process(
                COMMAND ${vcpkg_exe} upgrade --no-dry-run ${first_upgrade}
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        endif()
    endif()

    if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES x64 AND CMAKE_GENERATOR MATCHES "Visual Studio")
        set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "visual studio build architecture" FORCE)
    endif()

    if(WIN32 AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        if(VCPKG_TARGET_TRIPLET MATCHES "^x[68][46]-windows-")
            # set toolchain to VS for e.g. Ninja or jom
            set(CMAKE_C_COMPILER   cl CACHE STRING "Microsoft C/C++ Compiler" FORCE)
            set(CMAKE_CXX_COMPILER cl CACHE STRING "Microsoft C/C++ Compiler" FORCE)
        elseif(VCPKG_TARGET_TRIPLET MATCHES "^x[68][46]-mingw-")
            # set toolchain to MinGW for e.g. Ninja or jom
            set(CMAKE_C_COMPILER   gcc CACHE STRING "MinGW GCC C Compiler"   FORCE)
            set(CMAKE_CXX_COMPILER g++ CACHE STRING "MinGW G++ C++ Compiler" FORCE)
        endif()
    endif()

    set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake CACHE FILEPATH "vcpkg toolchain" FORCE)
endfunction()

vcpkg_set_toolchain()

include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
