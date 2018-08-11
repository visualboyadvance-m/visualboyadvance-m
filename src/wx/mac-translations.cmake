# install localizations
file(GLOB gmos "po/wxvbam/*.gmo")

foreach(gmo ${gmos})
    # strip path
    string(REGEX REPLACE "^.*/" "" gmo ${gmo})

    string(REPLACE ".gmo" "" lang ${gmo})

    file(MAKE_DIRECTORY "visualboyadvance-m.app/Contents/Resources/${lang}.lproj")
    file(COPY "po/wxvbam/${gmo}" DESTINATION "visualboyadvance-m.app/Contents/Resources/${lang}.lproj")
    file(RENAME "visualboyadvance-m.app/Contents/Resources/${lang}.lproj/${gmo}" "visualboyadvance-m.app/Contents/Resources/${lang}.lproj/wxvbam.mo")
endforeach()
