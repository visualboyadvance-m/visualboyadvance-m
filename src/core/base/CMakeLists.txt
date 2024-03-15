# This defines the `vbam-core-base` target.
# `vbam-core-base` is a collection of utilies and definitions use by the core
# and frontends.

add_library(vbam-core-base OBJECT)

target_sources(vbam-core-base
    PRIVATE
    file_util.cpp
    file_util_common.cpp
    internal/file_util_internal.cpp
    internal/file_util_internal.h
    internal/memgzio.c
    internal/memgzio.h

    PUBLIC
    file_util.h
    message.h
)

target_include_directories(vbam-core-base
    PUBLIC ${ZLIB_INCLUDE_DIR}
)

target_link_libraries(vbam-core-base
    INTERFACE vbam-fex
    PUBLIC ${ZLIB_LIBRARY}
)
