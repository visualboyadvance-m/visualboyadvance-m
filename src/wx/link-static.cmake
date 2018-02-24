file(READ CMakeFiles/visualboyadvance-m.dir/link.txt link_cmd)

string(REGEX REPLACE "-l(z|expat|X[^ ]+|xcb[^ ]*) " "-Wl,--whole-archive -l\\1 -Wl,--no-whole-archive " link_cmd ${link_cmd})

file(WRITE CMakeFiles/visualboyadvance-m.dir/link.txt ${link_cmd})
