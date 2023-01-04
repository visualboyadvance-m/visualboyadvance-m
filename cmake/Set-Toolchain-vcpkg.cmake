set(VCPKG_TARGET_TRIPLET "x64-windows-static-md")
set(VBAM_STATIC_DEFAULT ON)

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
        set(VBAM_VCPKG_PLATFORM ${VBAM_VCPKG_PLATFORM}-static-md)
    endif()

    set(VCPKG_TARGET_TRIPLET ${VBAM_VCPKG_PLATFORM} CACHE STRING "Vcpkg target triplet (ex. x86-windows)" FORCE)
    message(STATUS "Inferred VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
endif()

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

    find_program(vcpkg_exe NAMES vcpkg)

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

##TODO:Actually add these features in the manifest (options.cmake should be loaded before this here)
if(ENABLE_LINK)
    list(APPEND VCPKG_MANIFEST_FEATURES sfml)
endif()
if(ENABLE_FFMPEG)
    list(APPEND VCPKG_MANIFEST_FEATURES ffmpeg)
endif()
if(ENABLE_NLS)
    list(APPEND VCPKG_MANIFEST_FEATURES nls)
endif()
if(ENABLE_ONLINEUPDATES)
    list(APPEND VCPKG_MANIFEST_FEATURES update)
endif()

#set(VCPKG_INSTALL_OPTIONS --allow-unsupported)
#set(VCPKG_INSTALL_OPTIONS --no-print-usage)

vcpkg_set_toolchain()

if(NOT DEFINED VCPKG_HOST_TRIPLET AND NOT CMAKE_CROSSCOMPILING)
    set(VCPKG_HOST_TRIPLET "${VCPKG_TARGET_TRIPLET}")
endif()

include("${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
