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
    scalefx_simd.h
)

target_include_directories(vbam-components-filters-scalefx
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
