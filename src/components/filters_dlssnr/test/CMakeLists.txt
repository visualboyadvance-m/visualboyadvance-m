# apply_bench: times dlssnr::Filter::Apply32() over real game frames.
#
# The per-frame composition is what sets the frame rate with this filter, and it
# scales with the output extent rather than the model's, so it has to be measured
# behind a real display filter at a real size. It replays a frame_dump stream, so
# it needs one and is not registered with ctest -- like the core suite runners.
add_executable(apply_bench apply_bench.cpp)
target_link_libraries(apply_bench PRIVATE
    vbam-components-filters
    vbam-components-filters-dlssnr
)
