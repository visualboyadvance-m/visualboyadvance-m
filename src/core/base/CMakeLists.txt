# This defines the `vbam-core-base` target.
# `vbam-core-base` is a collection of utilies and definitions use by the core
# and frontends.

# Generate version_gen.h
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1,\\2,\\3,0" VBAM_WIN_VERSION "${VBAM_VERSION}")
set(VBAM_GENERATED_VERSION_H ${VBAM_GENERATED_DIR}/core/base/version_gen.h)
add_custom_target(vbam-core-base-generated
    COMMAND ${CMAKE_COMMAND}
        -DVBAM_GENERATED_VERSION_H=${VBAM_GENERATED_VERSION_H}
        -DVBAM_VERSION=${VBAM_VERSION}
        -DVBAM_REVISION=${VBAM_REVISION}
        -DVBAM_VERSION_RELEASE=${VBAM_VERSION_RELEASE}
        -DVBAM_WIN_VERSION=${VBAM_WIN_VERSION}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/build-version.cmake
    BYPRODUCTS
        ${VBAM_GENERATED_VERSION_H}
    DEPENDS
        version_gen.h.in
        build-version.cmake
)

add_library(vbam-core-base OBJECT)

add_dependencies(vbam-core-base vbam-core-base-generated)

target_sources(vbam-core-base
    PRIVATE
    file_util.cpp
    file_util_common.cpp
    internal/file_util_internal.cpp
    internal/file_util_internal.h
    internal/memgzio.c
    internal/memgzio.h
    patch.cpp
    version.cpp

    PUBLIC
    array.h
    file_util.h
    message.h
    patch.h
    port.h
    ringbuffer.h
    sizes.h
)

target_include_directories(vbam-core-base
    PUBLIC ${ZLIB_INCLUDE_DIR}
)

target_link_libraries(vbam-core-base
    INTERFACE vbam-fex
    PUBLIC ${ZLIB_LIBRARY}
)
