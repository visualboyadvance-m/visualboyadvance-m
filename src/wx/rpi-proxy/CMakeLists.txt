# RPI Proxy - Cross-architecture filter plugin support
# This allows 64-bit VBA-M builds to load 32-bit RPI filter plugins

if(NOT WIN32)
    return()
endif()

include(ExternalProject)

# The proxy client is only needed in 64-bit builds
if(X86_64 OR ARM64)
    # Add the proxy client library
    add_library(vbam-rpi-proxy STATIC
        RpiProxyClient.cpp
        RpiProxyClient.h
        SharedFilterBuffer.h
        rpi-host-rc.h
    )

    target_include_directories(vbam-rpi-proxy PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(vbam-rpi-proxy
        ${wxWidgets_LIBRARIES}
        shlwapi
    )

    target_compile_definitions(vbam-rpi-proxy PRIVATE
        ${wxWidgets_DEFINITIONS}
    )

    # Directory where the 32-bit host will be built/found
    set(RPI_HOST_BUILD_DIR "${CMAKE_BINARY_DIR}/rpi-host-x86")
    set(RPI_HOST_EXE_OUTPUT "${RPI_HOST_BUILD_DIR}/vbam-rpi-host.exe")

    # Check if user provided a pre-built host exe path
    if(NOT DEFINED RPI_HOST_EXE_PATH)
        # Default locations to check for pre-built host
        if(EXISTS "${CMAKE_SOURCE_DIR}/win32-deps/rpi-host/vbam-rpi-host.exe")
            set(RPI_HOST_EXE_PATH "${CMAKE_SOURCE_DIR}/win32-deps/rpi-host/vbam-rpi-host.exe")
            message(STATUS "Using pre-built RPI Host from win32-deps: ${RPI_HOST_EXE_PATH}")
        endif()
    endif()

    # If no pre-built host found, try to build it automatically with MSVC
    if(NOT RPI_HOST_EXE_PATH AND MSVC)
        message(STATUS "Will build 32-bit RPI Host automatically using MSVC x86 toolchain")

        # Determine the generator to use for the 32-bit build
        # For Visual Studio generators, we use the same generator with -A Win32
        if(CMAKE_GENERATOR MATCHES "Visual Studio")
            set(RPI_HOST_GENERATOR "${CMAKE_GENERATOR}")
            set(RPI_HOST_GENERATOR_PLATFORM "Win32")
        elseif(CMAKE_GENERATOR STREQUAL "Ninja" OR CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
            # For Ninja, we need to find the x86 MSVC toolchain
            set(RPI_HOST_GENERATOR "Ninja")
            set(RPI_HOST_GENERATOR_PLATFORM "")
        else()
            set(RPI_HOST_GENERATOR "NMake Makefiles")
            set(RPI_HOST_GENERATOR_PLATFORM "")
        endif()

        # Create a minimal CMakeLists.txt for the host build
        # This avoids needing the full VBA-M CMake configuration
        set(RPI_HOST_CMAKELISTS_CONTENT [=[
cmake_minimum_required(VERSION 3.15)
project(vbam-rpi-host C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static CRT linking to avoid conflicts with plugins that use different CRT versions
# Some plugins use legacy msvcrt.dll while modern MSVC uses UCRT - static linking avoids this
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(vbam-rpi-host
    ${SOURCE_DIR}/src/wx/rpi-proxy/host/rpi-host-main.cpp
)

target_include_directories(vbam-rpi-host PRIVATE
    ${SOURCE_DIR}/src
)

target_link_libraries(vbam-rpi-host
    kernel32
)

# Note: We use main() not WinMain(), so don't set WIN32_EXECUTABLE
# The host hides its console window programmatically with FreeConsole()

install(TARGETS vbam-rpi-host RUNTIME DESTINATION .)
]=])

        # Write the CMakeLists.txt for the external project
        file(MAKE_DIRECTORY "${RPI_HOST_BUILD_DIR}")
        string(REPLACE "\${SOURCE_DIR}" "${CMAKE_SOURCE_DIR}" RPI_HOST_CMAKELISTS_FINAL "${RPI_HOST_CMAKELISTS_CONTENT}")
        file(WRITE "${RPI_HOST_BUILD_DIR}/CMakeLists.txt" "${RPI_HOST_CMAKELISTS_FINAL}")

        # Determine build type for the external project
        if(CMAKE_BUILD_TYPE)
            set(RPI_HOST_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
        else()
            set(RPI_HOST_BUILD_TYPE "Release")
        endif()

        # For Visual Studio generators, use ExternalProject with platform specification
        if(CMAKE_GENERATOR MATCHES "Visual Studio")
            ExternalProject_Add(vbam-rpi-host-build
                SOURCE_DIR "${RPI_HOST_BUILD_DIR}"
                BINARY_DIR "${RPI_HOST_BUILD_DIR}/build"
                CMAKE_GENERATOR "${RPI_HOST_GENERATOR}"
                CMAKE_GENERATOR_PLATFORM "${RPI_HOST_GENERATOR_PLATFORM}"
                CMAKE_ARGS
                    -DCMAKE_BUILD_TYPE=${RPI_HOST_BUILD_TYPE}
                    -DCMAKE_INSTALL_PREFIX=${RPI_HOST_BUILD_DIR}/install
                BUILD_COMMAND ${CMAKE_COMMAND} --build . --config ${RPI_HOST_BUILD_TYPE}
                INSTALL_COMMAND ${CMAKE_COMMAND} --install . --config ${RPI_HOST_BUILD_TYPE}
                BUILD_BYPRODUCTS "${RPI_HOST_BUILD_DIR}/install/vbam-rpi-host.exe"
                DOWNLOAD_COMMAND ""
                UPDATE_COMMAND ""
            )
            set(RPI_HOST_EXE_OUTPUT "${RPI_HOST_BUILD_DIR}/install/vbam-rpi-host.exe")
        else()
            # For Ninja/NMake, we need to set up the x86 environment
            # Find vcvarsall.bat to set up the x86 environment
            get_filename_component(MSVC_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

            # Try to find vcvarsall.bat
            # It's typically in VC/Auxiliary/Build relative to the VS installation
            set(VCVARSALL_SEARCH_PATHS
                "${MSVC_COMPILER_DIR}/../../../../../../Auxiliary/Build"
                "${MSVC_COMPILER_DIR}/../../../Auxiliary/Build"
                "$ENV{VSINSTALLDIR}/VC/Auxiliary/Build"
                "$ENV{VS2022INSTALLDIR}/VC/Auxiliary/Build"
                "$ENV{VS2019INSTALLDIR}/VC/Auxiliary/Build"
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build"
                "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build"
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build"
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Auxiliary/Build"
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/VC/Auxiliary/Build"
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build"
            )

            unset(VCVARSALL_BAT)
            foreach(SEARCH_PATH ${VCVARSALL_SEARCH_PATHS})
                if(EXISTS "${SEARCH_PATH}/vcvarsall.bat")
                    get_filename_component(VCVARSALL_BAT "${SEARCH_PATH}/vcvarsall.bat" ABSOLUTE)
                    break()
                endif()
            endforeach()

            if(VCVARSALL_BAT)
                message(STATUS "Found vcvarsall.bat: ${VCVARSALL_BAT}")

                # Create a batch script that sets up x86 environment and builds
                set(BUILD_SCRIPT_CONTENT "@echo off
call \"${VCVARSALL_BAT}\" x86
cd /d \"${RPI_HOST_BUILD_DIR}/build\"
cmake -G \"${RPI_HOST_GENERATOR}\" -DCMAKE_BUILD_TYPE=${RPI_HOST_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=\"${RPI_HOST_BUILD_DIR}/install\" \"${RPI_HOST_BUILD_DIR}\"
cmake --build . --config ${RPI_HOST_BUILD_TYPE}
cmake --install . --config ${RPI_HOST_BUILD_TYPE}
")
                file(WRITE "${RPI_HOST_BUILD_DIR}/build-x86.bat" "${BUILD_SCRIPT_CONTENT}")

                # Use add_custom_command/target instead of ExternalProject for better control
                file(MAKE_DIRECTORY "${RPI_HOST_BUILD_DIR}/build")

                add_custom_command(
                    OUTPUT "${RPI_HOST_BUILD_DIR}/install/vbam-rpi-host.exe"
                    COMMAND cmd /c "${RPI_HOST_BUILD_DIR}/build-x86.bat"
                    DEPENDS
                        "${CMAKE_CURRENT_SOURCE_DIR}/host/rpi-host-main.cpp"
                        "${CMAKE_CURRENT_SOURCE_DIR}/SharedFilterBuffer.h"
                    WORKING_DIRECTORY "${RPI_HOST_BUILD_DIR}"
                    COMMENT "Building 32-bit RPI Host with MSVC x86 toolchain"
                    VERBATIM
                )

                add_custom_target(vbam-rpi-host-build
                    DEPENDS "${RPI_HOST_BUILD_DIR}/install/vbam-rpi-host.exe"
                )

                set(RPI_HOST_EXE_OUTPUT "${RPI_HOST_BUILD_DIR}/install/vbam-rpi-host.exe")
            else()
                message(WARNING "Could not find vcvarsall.bat - cannot automatically build 32-bit RPI Host")
                message(WARNING "Please build vbam-rpi-host.exe manually with 32-bit toolchain")
            endif()
        endif()

        # Set the path to the built host exe (will be available after build)
        if(TARGET vbam-rpi-host-build)
            set(RPI_HOST_EXE_PATH "${RPI_HOST_EXE_OUTPUT}")
            set(RPI_HOST_AUTO_BUILD TRUE)

            # Make the proxy library depend on the host build
            add_dependencies(vbam-rpi-proxy vbam-rpi-host-build)
        endif()
    endif()

    # Configure the proxy based on whether we have a host exe
    if(RPI_HOST_EXE_PATH)
        if(RPI_HOST_AUTO_BUILD)
            message(STATUS "RPI Host will be built automatically: ${RPI_HOST_EXE_PATH}")
        else()
            message(STATUS "RPI Host exe found: ${RPI_HOST_EXE_PATH}")
        endif()

        set(VBAM_HAS_RPI_PROXY TRUE PARENT_SCOPE)
        # Export RPI_HOST_EXE_PATH to parent scope for resource dependency tracking
        set(RPI_HOST_EXE_PATH "${RPI_HOST_EXE_PATH}" PARENT_SCOPE)

        # Generate the resource include path header
        # Place in src/wx so the resource compiler can find it
        file(TO_NATIVE_PATH "${RPI_HOST_EXE_PATH}" RPI_HOST_EXE_NATIVE_PATH)
        string(REPLACE "\\" "/" RPI_HOST_EXE_RC_PATH "${RPI_HOST_EXE_NATIVE_PATH}")

        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/rpi-host-path.h.in
            ${CMAKE_SOURCE_DIR}/src/wx/rpi-host-path.h
            @ONLY
        )
    else()
        message(STATUS "RPI Host exe not found - 32-bit plugin support disabled")
        message(STATUS "To enable: build vbam-rpi-host.exe with 32-bit toolchain")
        message(STATUS "and set RPI_HOST_EXE_PATH or place in win32-deps/rpi-host/")
        set(VBAM_HAS_RPI_PROXY FALSE PARENT_SCOPE)
        # Define NO_RPI_PROXY so the resource compiler skips the RPI proxy section
        add_compile_definitions(NO_RPI_PROXY)
    endif()
else()
    # 32-bit builds don't use the proxy, they load plugins directly
    set(VBAM_HAS_RPI_PROXY FALSE PARENT_SCOPE)
endif()

# The host executable is built when we're in a 32-bit build
# It will be embedded as a resource in 64-bit builds
if(X86 AND NOT X86_64)
    add_subdirectory(host)
endif()
