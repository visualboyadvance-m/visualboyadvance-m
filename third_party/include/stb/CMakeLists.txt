# Defines the `STB_INCLUDE_DIR` variable and `stb-image` target.

# STB Image library dependency (header-only).
set(STB_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR} PARENT_SCOPE)

add_library(stb-image INTERFACE)

target_include_directories(stb-image INTERFACE
    ${STB_INCLUDE_DIR}
)

target_sources(stb-image INTERFACE
    stb_image.h
    stb_image_write.h
)
