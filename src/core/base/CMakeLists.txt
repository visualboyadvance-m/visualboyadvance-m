# This defines the `vbam-core-base` target.
# `vbam-core-base` is a collection of utilies and definitions use by the core
# and frontends.

# Generate version_gen.h
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1,\\2,\\3,0" VBAM_WIN_VERSION "${VBAM_VERSION}")
set(VBAM_GENERATED_VERSION_H ${VBAM_GENERATED_DIR}/core/base/version_gen.h)
add_custom_command(
    OUTPUT
        ${VBAM_GENERATED_VERSION_H}
    COMMAND ${CMAKE_COMMAND}
        -DVBAM_GENERATED_VERSION_H=${VBAM_GENERATED_VERSION_H}
        -DVBAM_VERSION=${VBAM_VERSION}
        -DVBAM_REVISION=${VBAM_REVISION}
        -DVBAM_VERSION_RELEASE=${VBAM_VERSION_RELEASE}
        -DVBAM_WIN_VERSION=${VBAM_WIN_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/build-version.cmake
    DEPENDS
        version_gen.h.in
        build-version.cmake
)

add_library(vbam-core-base OBJECT)

target_sources(vbam-core-base
    PRIVATE
    file_util_common.cpp
    file_util_desktop.cpp
    image_util.cpp
    internal/file_util_internal.cpp
    internal/file_util_internal.h
    internal/memgzio.c
    internal/memgzio.h
    patch.cpp
    version.cpp

    PUBLIC
    array.h
    file_util.h
    image_util.h
    message.h
    patch.h
    port.h
    ringbuffer.h
    sizes.h
    sound_driver.h
    system.h
    version.h
    # Generated file.
    ${VBAM_GENERATED_VERSION_H}
)

target_include_directories(vbam-core-base
    PRIVATE ${STB_INCLUDE_DIR}
    PUBLIC ${ZLIB_INCLUDE_DIR}
)

target_link_libraries(vbam-core-base
    PRIVATE vbam-fex stb-image
    PUBLIC ${ZLIB_LIBRARY}
)
