if(VCPKG_TARGET_TRIPLET)
    if(NOT DEFINED ENV{VCPKG_ROOT})
        get_filename_component(VCPKG_ROOT ${CMAKE_SOURCE_DIR}/../vcpkg ABSOLUTE)
        set(ENV{VCPKG_ROOT} ${VCPKG_ROOT})
    else()
        set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    endif()

    if(NOT EXISTS ${VCPKG_ROOT})
        get_filename_component(vcpkg_root_parent ${VCPKG_ROOT}/.. ABSOLUTE)

        execute_process(
            COMMAND git clone https://github.com/microsoft/vcpkg.git
            RESULT_VARIABLE git_status
            WORKING_DIRECTORY ${vcpkg_root_parent}
        )

        if(NOT git_status EQUAL 0)
            message(FATAL_ERROR "Error cloning vcpkg from git, please make sure git for windows is installed correctly, it can be installed from Visual Studio components")
        endif()
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
                
                if(NOT git_status EQUAL 0)
                    break()
                endif()
            endforeach()
        else()
            execute_process(
                COMMAND git pull --rebase
                RESULT_VARIABLE git_status
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        endif()

        if(NOT git_status EQUAL 0)
            message(FATAL_ERROR "Error updating vcpkg from git, please make sure git for windows is installed correctly, it can be installed from Visual Studio components")
        endif()
    endif()

    # build latest vcpkg
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

    foreach(pkg ${VCPKG_DEPS})
        list(APPEND VCPKG_DEPS_QUALIFIED ${pkg}:${VCPKG_TARGET_TRIPLET})
    endforeach()

    # build our deps
    if(WIN32)
        execute_process(
            COMMAND vcpkg install ${VCPKG_DEPS_QUALIFIED}
            WORKING_DIRECTORY ${VCPKG_ROOT}
        )
    else()
        execute_process(
            COMMAND ./vcpkg install ${VCPKG_DEPS_QUALIFIED}
            WORKING_DIRECTORY ${VCPKG_ROOT}
        )
    endif()

    if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES x64)
        set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "visual studio build architecture" FORCE)
    endif()

    set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake CACHE FILEPATH "vcpkg toolchain" FORCE)
    include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
endif()
