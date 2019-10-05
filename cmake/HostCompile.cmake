function(host_compile src dst_cmd)
    unset(link_flags)

    if(CMAKE_HOST_WIN32)
        if(NOT dst MATCHES "\\.[Ee][Xx][Ee]\$")
            set(dst "${dst_cmd}.exe")
        endif()

        if(CMAKE_COMPILER_IS_GNUCXX)
            set(link_flags -Wl,--subsystem,console)
        endif()
    else()
        set(dst "${dst_cmd}")
    endif()

    if(NOT MSVC)
        # assume cc foo.c -o foo # will work on most hosts
        set(compile_command cc ${src} -o ${dst} ${link_flags})
    else()
        # special case for Visual Studio
	set(compile_command ${CMAKE_C_COMPILER} ${src} /link "/out:${dst}")
    endif()

    add_custom_command(
        OUTPUT "${dst}"
        COMMAND ${compile_command}
        DEPENDS "${src}"
    )
endfunction()
