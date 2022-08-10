file(GLOB catalogs po/wxvbam/*.gmo)

file(MAKE_DIRECTORY translations)

foreach(catalog ${catalogs})
    string(REGEX REPLACE ".*/([^/]+)\\.gmo" "\\1" locale ${catalog})

    file(MAKE_DIRECTORY translations/${locale}/LC_MESSAGES)

    configure_file(${catalog} translations/${locale}/LC_MESSAGES/wxvbam.mo COPYONLY)
endforeach()

execute_process(
    COMMAND ${ZIP_PROGRAM} -9r ../translations.zip .
    WORKING_DIRECTORY translations
    OUTPUT_QUIET
)
