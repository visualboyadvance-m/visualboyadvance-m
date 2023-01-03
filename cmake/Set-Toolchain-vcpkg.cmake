if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    # Check if we are in an MSVC environment.
    if($ENV{CXX} MATCHES "cl.exe$")
        # Infer the architecture from the LIB folders.
        foreach(LIB $ENV{LIB})
            if(${LIB} MATCHES "x64$")
                set(VBAM_VCPKG_PLATFORM "x64-windows")
                break()
            endif()
            if(${LIB} MATCHES "x86$")
                set(VBAM_VCPKG_PLATFORM "x86-windows")
                break()
            endif()
            if(${LIB} MATCHES "ARM64$")
                set(VBAM_VCPKG_PLATFORM "arm64-windows")
                break()
            endif()
        endforeach()

        # If all else fails, try to use a sensible default.
        if(NOT DEFINED VBAM_VCPKG_PLATFORM)
            set(VBAM_VCPKG_PLATFORM "x64-windows")
        endif()

    elseif (NOT DEFINED CMAKE_CXX_COMPILER)
        # No way to infer the compiler.
        return()

    elseif(${CMAKE_CXX_COMPILER} MATCHES "clang-cl.exe$" OR ${CMAKE_CXX_COMPILER} MATCHES "clang-cl$")
        # For stand-alone clang-cl, assume x64.
        set(VBAM_VCPKG_PLATFORM "x64-windows")
    endif()

    if (NOT DEFINED VBAM_VCPKG_PLATFORM)
        # Probably not an MSVC environment.
        return()
    endif()

    if(DEFINED BUILD_SHARED_LIBS AND NOT ${BUILD_SHARED_LIBS})
        set(VBAM_VCPKG_PLATFORM ${VBAM_VCPKG_PLATFORM}-static)
    endif()

    set(VCPKG_TARGET_TRIPLET ${VBAM_VCPKG_PLATFORM} CACHE STRING "Vcpkg target triplet (ex. x86-windows)" FORCE)
    message(STATUS "Inferred VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
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
        message(WARNING "Error updating vcpkg from git, please make sure git for windows is installed correctly, it can be installed from Visual Studio components. git_status:${git_status}")
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

        find_package(Git REQUIRED)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone https://github.com/microsoft/vcpkg.git
            RESULT_VARIABLE git_status
            WORKING_DIRECTORY ${root_parent}
        )

        vcpkg_check_git_status(${git_status})
    else()
        # this is the case when we cache vcpkg/installed with the appveyor build cache
        if(NOT EXISTS ${VCPKG_ROOT}/.git)
            set(git_commands
                "${GIT_EXECUTABLE} init"
                "${GIT_EXECUTABLE} remote add origin https://github.com/microsoft/vcpkg.git"
                "${GIT_EXECUTABLE} fetch --all --prune"
                "${GIT_EXECUTABLE} reset --hard origin/master"
                "${GIT_EXECUTABLE} branch --set-upstream-to=origin/master master"
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
                COMMAND ${GIT_EXECUTABLE} fetch origin
                RESULT_VARIABLE git_status
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
            vcpkg_check_git_status(${git_status})

            execute_process(
                COMMAND ${GIT_EXECUTABLE} status
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
                    COMMAND ${GIT_EXECUTABLE} pull --rebase
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

    find_program(vcpkg_exe NAMES vcpkg)

    # update portfiles
    execute_process(
        COMMAND ${vcpkg_exe} update
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

        if("${val}" OR (seconds LESS time_limit AND ("${val}" OR "${val}" STREQUAL "")))
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

    if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES x64 AND CMAKE_GENERATOR MATCHES "Visual Studio")
        set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "visual studio build architecture" FORCE)
    endif()

    if(WIN32 AND NOT CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT DEFINED CMAKE_CXX_COMPILER)
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

    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain" FORCE)
endfunction()

vcpkg_set_toolchain()

if(NOT DEFINED VCPKG_HOST_TRIPLET)
    set(VCPKG_HOST_TRIPLET "${VCPKG_TARGET_TRIPLET}")
endif()

include("${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
