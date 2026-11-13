# Packaging of a classic Mac OS application for the Retro68 toolchains.
#
# Retro68's own add_application() creates the executable target itself; the
# wx port's target is made, and configured, long before, so this does the
# same packaging steps for an existing target:
#
#   68k (Retro68):      the linker (ld wrapping Elf2Mac) already writes the
#                       code resources; Rez puts them after Retro68APPL.r.
#   PowerPC (RetroPPC): MakePEF turns the XCOFF into a PEF code fragment, the
#                       application's data fork, after RetroPPCAPPL.r's cfrg.
#
# Then come the resources compiled from RESOURCES (.r files, or ready-made
# .rsrc/.rsrc.bin), which override the template's (its SIZE in particular).
# The result is <NAME>.bin (MacBinary, keeps the resource fork through any
# transfer), <NAME>.APPL (with the resource fork in the native one on a Mac
# host) and <NAME>.dsk, an HFS disk image with the application on it, for
# emulators such as Basilisk II, SheepShaver and QEMU. All three have the
# bundle bit set, so the Finder shows the application's own icon;
# Retro68FinishApplication.cmake does that, and makes the disk image.
#
# vbam_retro68_application(<target>
#     NAME <application name>
#     CREATOR <four-character creator code>
#     [TYPE <file type, default APPL>]
#     [RESOURCES <.r/.rsrc files>...]
#     [REZ_INCLUDES <directories>...])

include_guard(GLOBAL)

include(HostCompile)

function(vbam_retro68_application target)
    cmake_parse_arguments(ARG "" "NAME;CREATOR;TYPE" "RESOURCES;REZ_INCLUDES" ${ARGN})

    if(NOT ARG_NAME)
        set(ARG_NAME "${target}")
    endif()
    if(NOT ARG_TYPE)
        set(ARG_TYPE APPL)
    endif()
    if(NOT ARG_CREATOR)
        set(ARG_CREATOR "????")
    endif()

    if(NOT REZ OR NOT REZ_TEMPLATES_PATH)
        message(FATAL_ERROR "vbam_retro68_application(): REZ and REZ_TEMPLATES_PATH come from the Retro68 toolchain file")
    endif()

    set(rez_includes)
    foreach(dir ${REZ_INCLUDE_PATH} ${ARG_REZ_INCLUDES})
        list(APPEND rez_includes "-I${dir}")
    endforeach()

    set(rez_flags)
    if(CMAKE_SYSTEM_NAME STREQUAL "RetroCarbon")
        set(rez_flags -DTARGET_API_MAC_CARBON=1)
    endif()

    # The application's own resources, each compiled on its own first so a
    # change to one does not recompile the rest.
    set(rsrc_files)
    foreach(res ${ARG_RESOURCES})
        get_filename_component(res_path "${res}" ABSOLUTE)
        if(res_path MATCHES "\\.r$")
            get_filename_component(res_name "${res_path}" NAME)
            set(rsrc "${CMAKE_CURRENT_BINARY_DIR}/${res_name}.rsrc.bin")
            # Rez has no dependency output; the includes are listed by hand.
            set(res_globs)
            foreach(dir ${ARG_REZ_INCLUDES})
                list(APPEND res_globs "${dir}/*.r")
            endforeach()
            file(GLOB res_deps LIST_DIRECTORIES false ${res_globs})
            add_custom_command(
                OUTPUT "${rsrc}"
                COMMAND "${REZ}" ${rez_flags} "${res_path}" ${rez_includes} -o "${rsrc}"
                DEPENDS "${res_path}" ${res_deps}
                COMMENT "Compiling resources ${res_name}"
                VERBATIM
            )
            list(APPEND rsrc_files "${rsrc}")
        else()
            list(APPEND rsrc_files "${res_path}")
        endif()
    endforeach()

    # Sets the bundle bit Rez leaves clear; see Retro68FinishApplication.cmake.
    set(set_flags "${CMAKE_BINARY_DIR}/macbinary-set-flags")
    if(NOT TARGET retro68-macbinary-set-flags)
        host_compile("${CMAKE_SOURCE_DIR}/tools/retro68/macbinary-set-flags.c" "${set_flags}")
        add_custom_target(retro68-macbinary-set-flags DEPENDS "${set_flags}")
    endif()

    set(out "${CMAKE_BINARY_DIR}/${ARG_NAME}")
    # What Rez writes, before Retro68FinishApplication.cmake.
    set(rez_out "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.rez.bin")
    set(outputs "${out}.bin" "${out}.APPL" "${out}.dsk")

    set(finish
        COMMAND ${CMAKE_COMMAND}
                "-DINPUT=${rez_out}"
                "-DNAME=${ARG_NAME}"
                "-DOUT_BIN=${out}.bin"
                "-DOUT_DSK=${out}.dsk"
                "-DAPPL=${out}.APPL"
                "-DSET_FLAGS=${set_flags}"
                "-DTYPE=${ARG_TYPE}"
                "-DCREATOR=${ARG_CREATOR}"
                "-DHFSUTILS=${RETRO68_ROOT}/bin"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}"
                -P "${CMAKE_SOURCE_DIR}/cmake/Retro68FinishApplication.cmake"
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
        set_target_properties(${target} PROPERTIES OUTPUT_NAME "${target}.code.bin")

        add_custom_command(
            OUTPUT ${outputs}
            COMMAND "${REZ}" ${rez_flags}
                    "${REZ_TEMPLATES_PATH}/Retro68APPL.r"
                    ${rez_includes}
                    --copy "$<TARGET_FILE:${target}>"
                    -o "${rez_out}"
                    -t "${ARG_TYPE}" -c "${ARG_CREATOR}"
                    --cc "${out}.APPL"
                    ${rsrc_files}
            ${finish}
            DEPENDS ${target} ${rsrc_files} "${set_flags}"
                    "${CMAKE_SOURCE_DIR}/cmake/Retro68FinishApplication.cmake"
            COMMENT "Making the 68k application ${ARG_NAME}"
            VERBATIM
        )
    elseif(CMAKE_SYSTEM_NAME MATCHES "^Retro(PPC|Carbon)$")
        if(CMAKE_SYSTEM_NAME STREQUAL "RetroCarbon")
            set(template "${REZ_TEMPLATES_PATH}/RetroCarbonAPPL.r")
        else()
            set(template "${REZ_TEMPLATES_PATH}/RetroPPCAPPL.r")
        endif()

        set_target_properties(${target} PROPERTIES OUTPUT_NAME "${target}.xcoff")
        set(pef "${CMAKE_CURRENT_BINARY_DIR}/${target}.pef")

        add_custom_command(
            OUTPUT "${pef}"
            COMMAND "${MAKE_PEF}" "$<TARGET_FILE:${target}>" -o "${pef}"
            DEPENDS ${target}
            COMMENT "Making the PEF code fragment of ${ARG_NAME}"
            VERBATIM
        )

        add_custom_command(
            OUTPUT ${outputs}
            COMMAND "${REZ}" ${rez_flags}
                    "${template}"
                    ${rez_includes}
                    "-DCFRAG_NAME=\"${ARG_NAME}\""
                    -o "${rez_out}"
                    -t "${ARG_TYPE}" -c "${ARG_CREATOR}"
                    --cc "${out}.APPL"
                    --data "${pef}"
                    ${rsrc_files}
            ${finish}
            DEPENDS "${pef}" ${rsrc_files} "${set_flags}"
                    "${CMAKE_SOURCE_DIR}/cmake/Retro68FinishApplication.cmake"
            COMMENT "Making the PowerPC application ${ARG_NAME}"
            VERBATIM
        )
    else()
        message(FATAL_ERROR "vbam_retro68_application(): not a Retro68 toolchain (${CMAKE_SYSTEM_NAME})")
    endif()

    add_custom_target(${target}-application ALL DEPENDS ${outputs})
endfunction()
