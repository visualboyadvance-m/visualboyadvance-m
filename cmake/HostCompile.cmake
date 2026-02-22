function(host_compile src dst_cmd)
    if(CMAKE_CROSSCOMPILING)
        unset(link_flags)
        set(dst "${dst_cmd}")

        if(CMAKE_HOST_WIN32)
            if(CMAKE_COMPILER_IS_GNUCXX)
                set(link_flags -Wl,--subsystem,console)
            endif()
        endif()

        if(MSVC)
            set(msvc_compile_script ${CMAKE_SOURCE_DIR}/cmake/MSVC_x86_Host_Compile.cmake)

            add_custom_command(
                OUTPUT ${dst}
                DEPENDS ${src} ${msvc_compile_script}
                COMMAND ${CMAKE_COMMAND} -D "src=${src}" -D "dst=${dst}" -P ${msvc_compile_script}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        else()
            set(dst ${dst_cmd})
            set(cc cc)

            if(APPLE)
                set(cc /usr/bin/clang)
            endif()

            # Assume: cc foo.c -o foo # will work on most hosts
            add_custom_command(
                OUTPUT ${dst}
                DEPENDS ${src}
                COMMAND ${cc} ${src} -o ${dst} ${link_flags}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
        endif()
    else()
        get_filename_component(dst ${dst_cmd} NAME)

        add_executable(${dst} ${src})

        # this is necessary because we override main with SDL_main
        target_compile_definitions(${dst} PRIVATE -Dmain=main)
    endif()
endfunction()
