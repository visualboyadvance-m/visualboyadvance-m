if (NOT MINGW)
    return()
endif()

# this has to run after the toolchain is initialized.
include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-include")
include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")

# link libgcc/libstdc++ statically on GCC/mingw
# and adjust link command when making a static binary
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND VBAM_STATIC)
    # some dists don't have a static libpthread
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread ")

    if(WIN32)
        add_custom_command(
            TARGET visualboyadvance-m PRE_LINK
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/msys-link-static.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    else()
        add_custom_command(
            TARGET visualboyadvance-m PRE_LINK
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/link-static.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
endif()