# xbrz_test: the xBRZ scaler's SSE2 / NEON build against its scalar backend.
#
# The reference is the very same xbrz.cpp compiled a second time with
# XBRZ_FORCE_SCALAR (xbrz_simd.h then emulates the four lanes in plain
# loops) and with the namespace renamed on the command line, so both copies
# can be linked into one executable. xbrz_ref_shim.cpp wraps the renamed
# entry point in a plain-int signature for the test to call.
add_library(xbrz-scalar-ref OBJECT
    ../internal/xBRZ/xbrz.cpp
    xbrz_ref_shim.cpp
)
target_compile_definitions(xbrz-scalar-ref PRIVATE XBRZ_FORCE_SCALAR xbrz=xbrz_scalar_ref)
target_compile_options(xbrz-scalar-ref PRIVATE ${XBRZ_COMPILE_OPTIONS})

add_executable(xbrz_test
    xbrz_test.cpp
    ../internal/xBRZ/xbrz.cpp
)
set_source_files_properties(../internal/xBRZ/xbrz.cpp PROPERTIES COMPILE_OPTIONS "${XBRZ_COMPILE_OPTIONS}")
target_link_libraries(xbrz_test PRIVATE xbrz-scalar-ref GTest::gtest_main)

if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    vbam_gtest_discover_tests(xbrz_test)
endif()
