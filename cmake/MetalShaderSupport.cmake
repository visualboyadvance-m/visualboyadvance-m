# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENCE.txt or https://cmake.org/licensing for details.

function(add_metal_shader_library TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 _amsl
        ""
        "STANDARD"
        ""
    )

    add_library(${TARGET} MODULE ${_amsl_UNPARSED_ARGUMENTS})

    set_target_properties(${TARGET} PROPERTIES
        DEBUG_POSTFIX ""
        XCODE_PRODUCT_TYPE com.apple.product-type.metal-library
        XCODE_ATTRIBUTE_MTL_FAST_MATH "YES"
        XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO[variant=Debug] "INCLUDE_SOURCE"
        XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO[variant=RelWithDebInfo] "INCLUDE_SOURCE"
        XCODE_ATTRIBUTE_MTL_HEADER_SEARCH_PATHS "$(HEADER_SEARCH_PATHS)"
    )

    if(_amsl_STANDARD AND _amsl_STANDARD MATCHES "metal([0-9]+)\.([0-9]+)")
        target_compile_options(${TARGET}
            PRIVATE "-std=${_amsl_STANDARD}"
        )

        set_target_properties(${TARGET} PROPERTIES
            XCODE_ATTRIBUTE_MTL_LANGUAGE_REVISION "Metal${CMAKE_MATCH_1}${CMAKE_MATCH_2}"
        )
    endif()
endfunction()

function(target_embed_metal_shader_libraries TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 _temsl
        ""
        ""
        ""
    )

    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.28 AND ${CMAKE_GENERATOR} STREQUAL "Xcode")
        set_target_properties(${TARGET} PROPERTIES
            XCODE_EMBED_RESOURCES "${_temsl_UNPARSED_ARGUMENTS}"
        )
    else()
        foreach(SHADERLIB IN LISTS _temsl_UNPARSED_ARGUMENTS)
            add_dependencies(${TARGET} ${SHADERLIB})
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${SHADERLIB}>" "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET}>/Resources/$<TARGET_FILE_NAME:${SHADERLIB}>"
                VERBATIM
            )
        endforeach()
    endif()
endfunction()
