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

# The composition kernels are templates on bool, and an instantiation that
# does not use a weight initializes it to a constant 0, so conditions like
# `w > 0` fold to a constant there. MSVC 14.4x (the v143 toolset) reports
# those as C4127, which /WX makes fatal; 14.5x, the toolset this project's
# warning baseline was set with, does not report them at all. Only two of the
# six sites have a condition made of template parameters alone and could be
# written `if constexpr` -- in the rest a weight is a real runtime value in
# the instantiations that read it, so there is nothing else to write.
if(MSVC)
    target_compile_options(vbam-components-filters-dlssnr PRIVATE
        /wd4127 # conditional expression is constant
    )
endif()

target_link_libraries(vbam-components-filters-dlssnr PUBLIC dlssnr)

if(BUILD_TESTING AND NOT COMPILE_ONLY)
    add_subdirectory(test)
endif()
