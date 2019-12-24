file(GLOB po_files "${CMAKE_CURRENT_SOURCE_DIR}/*.po")

gettext_create_translations(
	wxvbam.pot
        ${po_files}
)

add_dependencies(visualboyadvance-m translations)
