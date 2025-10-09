add_library(vbam-components-filters-interframe OBJECT)

target_sources(vbam-components-filters-interframe
    PRIVATE interframe.cpp
    PUBLIC interframe.h
)
