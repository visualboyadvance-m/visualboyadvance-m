# This defines the `vbam-core-fake` library, which is used for providing a fake
# implementation of the core library for testing purposes.

if(NOT BUILD_TESTING)
    return()
endif()

add_library(vbam-core-fake OBJECT)

target_sources(vbam-core-fake
    PRIVATE
    fake_core.cpp
)
