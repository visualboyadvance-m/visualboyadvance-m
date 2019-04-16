if(WIN32 OR CMAKE_TOOLCHAIN_FILE MATCHES "[Mm][Ii][Nn][Gg][Ww]")
    # compiler has not been detected yet maybe
    if(CMAKE_C_COMPILER MATCHES "cl\\.exe" OR CMAKE_CXX_COMPILER MATCHES "cl\\.exe" OR MSVC OR DEFINED ENV{VisualStudioVersion})
        set(VS TRUE)
    endif()

    set(WINARCH x86)
    if(CMAKE_C_COMPILER MATCHES x64 OR CMAKE_CXX_COMPILER MATCHES x64 OR "$ENV{VSCMD_ARG_TGT_ARCH}" MATCHES x64)
        set(WINARCH x64)
    endif()

    # Win32 deps submodules (dependencies and vcpkg)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include" OR NOT EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(git_checkout FALSE)

        if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
            set(git_checkout TRUE)
            execute_process(
                COMMAND git submodule update --init --remote --recursive
                RESULT_VARIABLE git_status
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            )
        endif()

        if(NOT (git_checkout AND git_status EQUAL 0))
            message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --init --remote --recursive")
        endif()
    endif()

    if(VS)
        set(DEPS_MSVC "${CMAKE_SOURCE_DIR}/dependencies/msvc")
        include_directories("${DEPS_MSVC}") # for GL/glext.h and getopt.h
    endif()

    if(VS AND ENABLE_VCPKG)
        if(NOT DEFINED ENV{VCPKG_ROOT})
            set(ENV{VCPKG_ROOT} "${CMAKE_SOURCE_DIR}/vcpkg")
        endif()

        # build vcpkg if not built
        if(NOT EXISTS $ENV{VCPKG_ROOT}/vcpkg.exe)
            execute_process(
                COMMAND bootstrap-vcpkg.bat
                WORKING_DIRECTORY $ENV{VCPKG_ROOT}
            )
        endif()

        foreach(pkg ${VCPKG_DEPS})
            #list(APPEND VCPKG_DEPS_QUALIFIED ${pkg}:${WINARCH}-windows-static)
            list(APPEND VCPKG_DEPS_QUALIFIED ${pkg}:${WINARCH}-windows)
        endforeach()

        # build our deps
        execute_process(
            COMMAND vcpkg install ${VCPKG_DEPS_QUALIFIED}
            WORKING_DIRECTORY $ENV{VCPKG_ROOT}
        )

        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH '' FORCE)
        include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

        set(NLS_DEFAULT OFF)
        set(ENABLE_NLS OFF) # not sure why this is necessary
    endif()
endif()
