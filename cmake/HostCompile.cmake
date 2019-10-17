function(host_compile src dst_cmd)
    if(CMAKE_CROSSCOMPILING)
        unset(link_flags)

        if(CMAKE_HOST_WIN32)
            set(dst "${dst_cmd}.exe")

            if(CMAKE_COMPILER_IS_GNUCXX)
                set(link_flags -Wl,--subsystem,console)
            endif()
        else()
            set(dst ${dst_cmd})
        endif()

        # assume cc foo.c -o foo # will work on most hosts
        add_custom_command(
            OUTPUT ${dst}
            COMMAND cc ${src} -o ${dst} ${link_flags}
            DEPENDS ${src}
        )
    else()
        get_filename_component(dst ${dst_cmd} NAME)

        add_executable(${dst} ${src})

        # this is necessary because we override main with SDL_main
        target_compile_definitions(${dst} PRIVATE -Dmain=main)
    endif()
endfunction()
