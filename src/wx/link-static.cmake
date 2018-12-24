set(link_cmd_file CMakeFiles/visualboyadvance-m.dir/link.txt)

if(EXISTS ${link_cmd_file})
    file(READ ${link_cmd_file} link_cmd)

    string(REGEX REPLACE "-l(z|expat|X[^ ]+|xcb[^ ]*) " "-Wl,--whole-archive -l\\1 -Wl,--no-whole-archive " link_cmd ${link_cmd})

    file(WRITE ${link_cmd_file} ${link_cmd})
endif()
