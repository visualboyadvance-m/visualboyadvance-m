if (NOT MINGW)
    return()
endif()

# this has to run after the toolchain is initialized.
include_directories("${CMAKE_SOURCE_DIR}/win32-deps/mingw-include")
include_directories("${CMAKE_SOURCE_DIR}/win32-deps/mingw-xaudio/include")

# Add Winsock as the last library linked because of broken link precedence.
set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -lws2_32")
