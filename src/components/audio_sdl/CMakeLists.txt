add_library(vbam-components-audio-sdl OBJECT)

target_sources(vbam-components-audio-sdl
    PRIVATE audio_sdl.cpp
    PUBLIC audio_sdl.h
)

target_include_directories(vbam-components-audio-sdl
    PUBLIC ${SDL3_INCLUDE_DIRS}
)

target_link_libraries(vbam-components-audio-sdl
    PUBLIC vbam-core-base ${VBAM_SDL3_LIBS}
)
