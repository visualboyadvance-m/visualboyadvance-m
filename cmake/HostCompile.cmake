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
        set(compile_command cl ${src} /link "/out:${dst}")
    endif()

    execute_process(COMMAND ${compile_command} OUTPUT_VARIABLE compile_out ERROR_VARIABLE compile_out RESULT_VARIABLE compile_result)

    if(NOT compile_result EQUAL 0)
        message(FATAL_ERROR "Failed compiling ${src} for the host: ${compile_out}")
    endif()
endfunction()
