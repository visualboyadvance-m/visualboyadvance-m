add_compile_definitions(${VBAM_FRONTEND_DEFINITIONS})

add_subdirectory(av_recording)
add_subdirectory(draw_text)
add_subdirectory(filters)
add_subdirectory(filters_agb)
add_subdirectory(filters_cgb)
add_subdirectory(filters_interframe)
add_subdirectory(filters_scalefx)
if(TARGET nr_frame)
    # DLSS NR needs libnr_frame from third_party/dlss-nr-on-vulkan (ENABLE_VULKAN).
    add_subdirectory(filters_dlssnr)
endif()
add_subdirectory(user_config)
