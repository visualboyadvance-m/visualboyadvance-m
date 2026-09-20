# DLSS NR (neural rendering) display filter, on top of libnr_frame from
# third_party/dlss-nr-on-vulkan. Only built when that tree is in the build
# (ENABLE_VULKAN), so the parent checks `if(TARGET nr_frame)` before adding
# this directory. Consumers that link this object library get the nr_frame
# shared library, its header directory and VBAM_ENABLE_DLSS_NR.

add_library(vbam-components-filters-dlssnr OBJECT)

target_sources(vbam-components-filters-dlssnr
    PRIVATE
    dlssnr.cpp

    PUBLIC
    dlssnr.h
)

target_include_directories(vbam-components-filters-dlssnr
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/dlss-nr-on-vulkan/src/ref
)

target_compile_definitions(vbam-components-filters-dlssnr PUBLIC VBAM_ENABLE_DLSS_NR)

# libnr_frame dlopens libxmx from its own directory, so the two must travel
# together; linking both keeps the bundling tools copying both.
target_link_libraries(vbam-components-filters-dlssnr PUBLIC nr_frame xmx)
