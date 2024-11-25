# Do not use this file directly.  Always use the top level CMakeLists.txt file
# File extractors so the user doesn't have to extract the rom before playing it.
# This defines the `vbam-fex` target.

add_library(vbam-fex OBJECT)

target_sources(vbam-fex
    PRIVATE
    7z_C/7z.h
    7z_C/7zAlloc.c
    7z_C/7zAlloc.h
    7z_C/7zBuf.c
    7z_C/7zBuf.h
    7z_C/7zCrc.c
    7z_C/7zCrc.h
    7z_C/7zCrcOpt.c
    7z_C/7zDec.c
    7z_C/7zIn.c
    7z_C/7zStream.c
    7z_C/Bcj2.c
    7z_C/Bcj2.h
    7z_C/Bra.c
    7z_C/Bra.h
    7z_C/Bra86.c
    7z_C/CpuArch.c
    7z_C/CpuArch.h
    7z_C/Lzma2Dec.c
    7z_C/Lzma2Dec.h
    7z_C/LzmaDec.c
    7z_C/LzmaDec.h
    7z_C/Ppmd.h
    7z_C/Ppmd7.c
    7z_C/Ppmd7.h
    7z_C/Ppmd7Dec.c
    7z_C/Types.h
    fex/Binary_Extractor.cpp
    fex/Binary_Extractor.h
    fex/blargg_common.cpp
    fex/blargg_common.h
    fex/blargg_config.h
    fex/blargg_endian.h
    fex/blargg_errors.cpp
    fex/blargg_errors.h
    fex/blargg_source.h
    fex/Data_Reader.cpp
    fex/Data_Reader.h
    fex/fex.cpp
    fex/File_Extractor.cpp
    fex/File_Extractor.h
    fex/Gzip_Extractor.cpp
    fex/Gzip_Extractor.h
    fex/Gzip_Reader.cpp
    fex/Gzip_Reader.h
    fex/Rar_Extractor.cpp
    fex/Rar_Extractor.h
    fex/Zip7_Extractor.cpp
    fex/Zip7_Extractor.h
    fex/Zip_Extractor.cpp
    fex/Zip_Extractor.h
    fex/Zlib_Inflater.cpp
    fex/Zlib_Inflater.h

    PUBLIC
    fex.h
)

target_include_directories(vbam-fex
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${ZLIB_INCLUDE_DIRS}
)

target_link_libraries(vbam-fex
    PRIVATE ${ZLIB_LIBRARY}
)

configure_vbam_target(vbam-fex)
