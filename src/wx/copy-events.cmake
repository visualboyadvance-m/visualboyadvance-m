# Create cmdtab.cpp, cmdhandlers.h, cmd-evtable.h from cmdevents.cpp

IF(NOT OUTDIR)
    SET(OUTDIR ".")
ENDIF(NOT OUTDIR)

SET(CMDTAB "${OUTDIR}/cmdtab.cpp")
SET(EVPROTO "${OUTDIR}/cmdhandlers.h")
SET(EVTABLE "${OUTDIR}/cmd-evtable.h")

FILE(READ cmdevents.cpp MW)
STRING(REGEX MATCHALL "\nEVT_HANDLER([^\")]|\"[^\"]*\")*\\)" MW "${MW}")

# cmdtab.cpp is a table of cmd-id-name/cmd-name pairs
# sorted for binary searching
FILE(WRITE "${CMDTAB}" "// Generated from cmdevents.cpp; do not edit\n#include \"wxvbam.h\"\n\nstruct cmditem cmdtab[] = {\n")
SET(EVLINES )
FOREACH(EV ${MW})
    # stripping the wxID_ makes it look better, but it's still all-caps
    STRING(REGEX REPLACE "^[^\"]*\\((wxID_|)([^,]*),[^\"]*(\"[^\"]*\")[^,)]*(,[^)]*|).*"
                         "    new_cmditem(wxT(\"\\2\"), wxTRANSLATE(\\3), XRCID(\"\\1\\2\")\\4 )"
			 EV "${EV}")
    STRING(REGEX REPLACE "XRCID\\(\"(wxID_[^\"]*)\"\\)" "\\1" EV ${EV})
    LIST(APPEND EVLINES "${EV},\n")
ENDFOREACH(EV)
LIST(SORT EVLINES)
STRING(REGEX REPLACE ",\n\$" "\n" EVLINES "${EVLINES}")
FILE(APPEND "${CMDTAB}" ${EVLINES})
FILE(APPEND "${CMDTAB}" "};\nconst int ncmds = sizeof(cmdtab) / sizeof(cmdtab[0]);\n")

# cmdhandlers.h contains prototypes for all handlers
FILE(WRITE "${EVPROTO}" "// Generated from cmdevents.cpp; do not edit\n")
FOREACH(EV ${MW})
    STRING(REGEX REPLACE "^[^\"]*\\(" "void On" P1 "${EV}")
    STRING(REGEX REPLACE ",.*" "(wxCommandEvent&);\n" P1 "${P1}")
    FILE(APPEND "${EVPROTO}" "${P1}")
    IF(EV MATCHES "EVT_HANDLER_MASK")
        STRING(REGEX REPLACE "^[^\"]*\\(" "void Do" P1 "${EV}")
        STRING(REGEX REPLACE ",.*" "();\n" P1 "${P1}")
        FILE(APPEND "${EVPROTO}" "${P1}")
    ENDIF(EV MATCHES "EVT_HANDLER_MASK")
ENDFOREACH(EV)

# cmd-evtable.h has the event table entries for all handlers
FILE(WRITE "${EVTABLE}" "// Generated from cmdevents.cpp; do not edit\n")
FOREACH(EV ${MW})
    FILE(APPEND "${EVTABLE}" "EVT_MENU(")
    STRING(REGEX REPLACE "[^\"]*\\(" "" EV "${EV}")
    STRING(REGEX REPLACE ",.*" "" EV "${EV}")
    IF("${EV}" MATCHES "wx.*")
      FILE(APPEND "${EVTABLE}" "${EV}")
    ELSE("${EV}" MATCHES "wx.*")
      FILE(APPEND "${EVTABLE}" "XRCID(\"${EV}\")")
    ENDIF("${EV}" MATCHES "wx.*")
    FILE(APPEND "${EVTABLE}" ", MainFrame::On${EV})\n")
ENDFOREACH(EV)
