add_library(vbam-components-filters-agb OBJECT)

target_sources(vbam-components-filters-agb
    PRIVATE filters_agb.cpp
    PUBLIC filters_agb.h
)
