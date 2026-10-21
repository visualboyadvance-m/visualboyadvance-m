add_library(vbam-components-filters-scalefx OBJECT)

target_sources(vbam-components-filters-scalefx
    PRIVATE
    scalefx.cpp
    scalefx_manager.cpp
    scalefx_pass0.cpp
    scalefx_pass1.cpp
    scalefx_pass2.cpp
    scalefx_pass3.cpp
    scalefx_pass4.cpp

    PUBLIC
    scalefx.h
    scalefx_manager.h
    scalefx_internal.h
)

target_include_directories(vbam-components-filters-scalefx
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# The passes keep float arithmetic in a fixed order so that the SSE2, NEON and
# scalar kernels produce identical bits (the unit test compares them); the
# release flags' -ffast-math would reassociate it.
if(MSVC)
    target_compile_options(vbam-components-filters-scalefx PRIVATE /fp:precise)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vbam-components-filters-scalefx PRIVATE -fno-fast-math -ffp-contract=off)
endif()
