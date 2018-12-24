set(link_cmd_file CMakeFiles/visualboyadvance-m.dir/linklibs.rsp)

if(EXISTS ${link_cmd_file})
    file(READ ${link_cmd_file} link_libs)

    string(REPLACE "-Wl,-Bdynamic" "" link_libs ${link_libs})

    file(WRITE ${link_cmd_file} ${link_libs})
endif()
