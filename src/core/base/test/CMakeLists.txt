# This defines the `vbam-core-base-test` library.

if(NOT BUILD_TESTING)
    return()
endif()

add_library(vbam-core-base-test
    INTERFACE
    notreached.h)

target_link_libraries(vbam-core-base-test
    INTERFACE GTest::gtest
    PRIVATE nonstd-lib)
