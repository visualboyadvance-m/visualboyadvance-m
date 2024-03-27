add_subdirectory(apu)
add_subdirectory(base)
add_subdirectory(fex)

# The vbam-core target contains both the Game Boy and Game Boy Advance
# emulators. These should be broken down into 2 separate targets. The issue lies
# with the Link and Sound emulation, which are tangled together between the two
# systems.
add_library(vbam-core STATIC)

target_sources(vbam-core
    PRIVATE
    # Game Boy
    gb/internal/gbCodes.h
    gb/internal/gbCodesCB.h
    gb/gb.cpp
    gb/gbCartData.cpp
    gb/gbCheats.cpp
    gb/gbDis.cpp
    gb/gbGfx.cpp
    gb/gbGlobals.cpp
    gb/gbMemory.cpp
    gb/gbPrinter.cpp
    gb/gbSGB.cpp
    gb/gbSound.cpp

    # Game Boy Advance
    # Built with flex -o debugger-expr-lex.cpp -P dexp_ debugger-expr.l
    gba/debugger-expr-lex.cpp
    # Built with bison -L c -o debugger-expr-yacc.cpp -H -p dexp_ debugger-expr.y
    # The yyerrorlab label handler was manually commented out to silence a
    # compiler warning.
    gba/debugger-expr-yacc.cpp
    gba/gba.cpp
    gba/gbaCpuArm.cpp
    gba/gbaCpuArmDis.cpp
    gba/gbaCpuThumb.cpp
    gba/gbaCheats.cpp
    gba/gbaCheatSearch.cpp
    gba/gbaEeprom.cpp
    gba/gbaElf.cpp
    gba/gbaFlash.cpp
    gba/gbaGfx.cpp
    gba/gbaGlobals.cpp
    gba/gbaMode0.cpp
    gba/gbaMode1.cpp
    gba/gbaMode2.cpp
    gba/gbaMode3.cpp
    gba/gbaMode4.cpp
    gba/gbaMode5.cpp
    gba/gbaPrint.cpp
    gba/gbaRtc.cpp
    gba/gbaSound.cpp
    gba/internal/gbaBios.cpp
    gba/internal/gbaBios.h
    gba/internal/gbaEreader.cpp
    gba/internal/gbaEreader.h
    gba/internal/gbaSram.cpp
    gba/internal/gbaSram.h

    PUBLIC
    # Game Boy
    gb/gb.h
    gb/gbCartData.h
    gb/gbCheats.h
    gb/gbDis.h
    gb/gbGfx.h
    gb/gbGlobals.h
    gb/gbMemory.h
    gb/gbPrinter.h
    gb/gbSGB.h
    gb/gbSound.h

    # Game Boy Advance
    gba/gba.h
    gba/gbaCheats.h
    gba/gbaCheatSearch.h
    gba/gbaCpu.h
    gba/gbaCpuArmDis.h
    gba/gbaEeprom.h
    gba/gbaElf.h
    gba/gbaFlash.h
    gba/gbaGfx.h
    gba/gbaGlobals.h
    gba/gbaInline.h
    gba/gbaPrint.h
    gba/gbaRtc.h
    gba/gbaSound.h
)

target_include_directories(vbam-core
    PUBLIC ${ZLIB_INCLUDE_DIR}
)

target_link_libraries(vbam-core
    PRIVATE vbam-core-apu vbam-fex
    PUBLIC vbam-core-base ${ZLIB_LIBRARY}
)

if(ENABLE_DEBUGGER)
    target_sources(vbam-core
        PRIVATE
        gba/gbaRemote.cpp
        gba/internal/gbaBreakpoint.cpp
        gba/internal/gbaBreakpoint.h

        PUBLIC
        gba/gbaRemote.h
    )
endif()

if(ENABLE_LINK)
    target_sources(vbam-core
        PRIVATE
        gba/gbaLink.cpp
        gba/internal/gbaSockClient.cpp
        gba/internal/gbaSockClient.h

        PUBLIC
        gba/gbaLink.h
    )

    target_include_directories(vbam-core
        PRIVATE ${SFML_INCLUDE_DIR}
    )

    target_link_libraries(vbam-core
        PRIVATE ${SFML_LIBRARIES}
    )

    # Add libintl.h
    target_include_directories(vbam-core
        PRIVATE ${LIBINTL_INC}
    )
    target_link_libraries(vbam-core
        PRIVATE ${NLS_LIBS}
    )
endif()
