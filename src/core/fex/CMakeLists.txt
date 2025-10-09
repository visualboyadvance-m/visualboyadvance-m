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
    7z_C/7zArcIn.c
    7z_C/7zStream.c
    7z_C/Bcj2.c
    7z_C/Bcj2.h
    7z_C/Bra.c
    7z_C/Bra.h
    7z_C/Bra86.c
    7z_C/CpuArch.c
    7z_C/CpuArch.h
    7z_C/Delta.c
    7z_C/Delta.h
    7z_C/Lzma2Dec.c
    7z_C/Lzma2Dec.h
    7z_C/LzmaDec.c
    7z_C/LzmaDec.h
    7z_C/Ppmd.h
    7z_C/Ppmd7.c
    7z_C/Ppmd7.h
    7z_C/Ppmd7Dec.c
    7z_C/Ppmd7aDec.c
    7z_C/Ppmd8.c
    7z_C/Ppmd8.h
    7z_C/Ppmd8Dec.c
    7z_C/7zTypes.h
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
    fex/BZ2_Extractor.cpp
    fex/BZ2_Extractor.h
    fex/BZ2_Reader.cpp
    fex/BZ2_Reader.h
    fex/XZ_Extractor.cpp
    fex/XZ_Extractor.h
    fex/XZ_Reader.cpp
    fex/XZ_Reader.h
    fex/LZ_Extractor.cpp
    fex/LZ_Extractor.h
    fex/LZ_Reader.cpp
    fex/LZ_Reader.h
    fex/Rar_Extractor.cpp
    fex/Rar_Extractor.h
    fex/Tar_Extractor.cpp
    fex/Tar_Extractor.h
    fex/Zip7_Extractor.cpp
    fex/Zip7_Extractor.h
    fex/Zip_Extractor.cpp
    fex/Zip_Extractor.h
    fex/Zlib_Inflater.cpp
    fex/Zlib_Inflater.h
    fex/LZMA_Inflater.cpp
    fex/LZMA_Inflater.h
    fex/BZ2_Inflater.cpp
    fex/BZ2_Inflater.h
    unrar/archive.cpp
    unrar/arcread.cpp
    unrar/blake2s_sse.cpp
    unrar/blake2s.cpp
    unrar/blake2sp.cpp
    unrar/coder.cpp
    unrar/crc.cpp
    unrar/encname.cpp
    unrar/extract.cpp
    unrar/getbits.cpp
    unrar/hash.cpp
    unrar/headers.cpp
    unrar/model.cpp
    unrar/pathfn.cpp
    unrar/rarvm.cpp
    unrar/rarvmtbl.cpp
    unrar/rawread.cpp
    unrar/secpassword.cpp
    unrar/strfn.cpp
    unrar/suballoc.cpp
    unrar/timefn.cpp
    unrar/unicode.cpp
    unrar/unpack.cpp
    unrar/unpack15.cpp
    unrar/unpack20.cpp
    unrar/unpack30.cpp
    unrar/unpack50.cpp
    unrar/unpack50frag.cpp
    unrar/unpackinline.cpp
    unrar/unrar_misc.cpp
    unrar/unrar_open.cpp
    unrar/unrar.cpp
    PUBLIC
    fex.h
)

target_include_directories(vbam-fex
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${ZLIB_INCLUDE_DIRS}
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_library(BZ2_LIBRARY NAMES bz2d bz2)
else()
    find_library(BZ2_LIBRARY bz2)
endif()

find_library(LZMA_LIBRARY lzma)

if (BZ2_LIBRARY AND ENABLE_BZ2)
    target_compile_definitions(vbam-fex PRIVATE FEX_ENABLE_BZ2=1)
    target_link_libraries(vbam-fex
        PRIVATE ${BZ2_LIBRARY}
    )
endif()

if (LZMA_LIBRARY AND ENABLE_LZMA)
    target_compile_definitions(vbam-fex PRIVATE FEX_ENABLE_LZMA=1)
    target_link_libraries(vbam-fex
        PRIVATE ${LZMA_LIBRARY}
    )
endif()

target_link_libraries(vbam-fex
    PRIVATE ${ZLIB_LIBRARY}
)
