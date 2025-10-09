add_library(vbam-components-draw-text OBJECT)

target_sources(vbam-components-draw-text
    PRIVATE draw_text.cpp
    PUBLIC draw_text.h
)

target_link_libraries(vbam-components-draw-text
    PRIVATE vbam-core-base
)
