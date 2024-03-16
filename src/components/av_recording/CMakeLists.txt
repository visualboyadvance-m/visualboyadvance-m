if(NOT ENABLE_FFMPEG)
    return()
endif()

add_library(vbam-components-av-recording OBJECT)

target_sources(vbam-components-av-recording
    PRIVATE av_recording.cpp
    PUBLIC av_recording.h
)

target_include_directories(vbam-components-av-recording
    PUBLIC ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(vbam-components-av-recording
    PUBLIC ${FFMPEG_LIBRARIES}
)
