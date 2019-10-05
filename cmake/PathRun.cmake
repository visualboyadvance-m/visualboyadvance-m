function(make_path_run_wrapper cmd target)
    get_filename_component(name "${target}" NAME)
    get_filename_component(cmd_resolved "${cmd}" REALPATH)
    get_filename_component(base_name "${cmd_resolved}" NAME)
    get_filename_component(dir_name  "${cmd_resolved}" DIRECTORY)

    set(source "${target}.c")

    configure_file(src/gcc-wrap.c.in "${source}")

    include(HostCompile)
    host_compile("${source}" "${target}")

    add_custom_target(generate_${name} DEPENDS "${CMAKE_BINARY_DIR}/${name}")
    add_dependencies(generate generate_${name})
endfunction()
