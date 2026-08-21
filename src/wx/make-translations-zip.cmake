# Drive this from the .po files in the source tree rather than the .gmo files
# in the build tree: the latter are cumulative, so a catalog that goes away
# leaves an orphaned .gmo behind and stays embedded forever. For the same
# reason the output tree is rebuilt from scratch each time.
file(GLOB po_files ${SRC_DIR}/po/wxvbam/*.po)

file(REMOVE_RECURSE translations)
file(MAKE_DIRECTORY translations)

foreach(po_file ${po_files})
    get_filename_component(locale ${po_file} NAME_WE)

    file(MAKE_DIRECTORY translations/${locale}/LC_MESSAGES)

    configure_file(po/wxvbam/${locale}.gmo
                   translations/${locale}/LC_MESSAGES/wxvbam.mo COPYONLY)
endforeach()

execute_process(
    COMMAND ${ZIP_PROGRAM} -9r ../translations.zip .
    WORKING_DIRECTORY translations
    OUTPUT_QUIET
)
