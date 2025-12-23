# RPI Host - 32-bit executable for loading RPI filter plugins
# This is built with the 32-bit toolchain and embedded in 64-bit VBA-M builds

if(NOT WIN32 OR NOT X86)
    message(STATUS "RPI Host is only built for 32-bit Windows")
    return()
endif()

add_executable(vbam-rpi-host
    rpi-host-main.cpp
)

target_include_directories(vbam-rpi-host PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Minimal dependencies - no wxWidgets needed for the host
target_link_libraries(vbam-rpi-host
    kernel32
)

# Use Windows subsystem to avoid console window, but keep main() as entry point
if(MSVC)
    target_link_options(vbam-rpi-host PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
else()
    target_link_options(vbam-rpi-host PRIVATE -mwindows)
endif()

# Strip in release mode
if(CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
    if(MINGW)
        target_link_options(vbam-rpi-host PRIVATE -s)
    endif()
endif()

# Install the host exe for 32-bit builds (for testing)
install(TARGETS vbam-rpi-host
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Copy to a known location so 64-bit builds can find it
add_custom_command(TARGET vbam-rpi-host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/rpi-host"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:vbam-rpi-host>" "${CMAKE_BINARY_DIR}/rpi-host/"
    COMMENT "Copying vbam-rpi-host.exe for 64-bit build embedding"
)
