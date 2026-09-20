# DLSS NR (neural rendering) display filter, on top of libdlssnr from
# third_party/dlss-nr-on-vulkan: one static archive holding the compute runtime
# (libxmx), the frame library (nr_frame), the embedded weights and shaders. Only
# built when that tree is in the build (ENABLE_VULKAN), so the parent checks
# `if(TARGET dlssnr)` before adding this directory. Consumers that link this
# object library get the archive, its header directory and VBAM_ENABLE_DLSS_NR.
# The archive links no Vulkan library: it finds vkGetInstanceProcAddr in the
# process (the renderer's MoltenVK or loader) or, when a Vulkan panel lends its
# instance, uses that one.

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

target_link_libraries(vbam-components-filters-dlssnr PUBLIC dlssnr)
