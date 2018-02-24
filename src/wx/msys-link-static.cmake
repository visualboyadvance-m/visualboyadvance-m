file(READ CMakeFiles/visualboyadvance-m.dir/linklibs.rsp link_libs)

string(REPLACE "-Wl,-Bdynamic" "" link_libs ${link_libs})

file(WRITE CMakeFiles/visualboyadvance-m.dir/linklibs.rsp ${link_libs})
