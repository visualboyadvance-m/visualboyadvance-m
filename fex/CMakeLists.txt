#Do not use this file directly.  Always use the top level CMakeLists.txt file
#File extractors so the user doesn't have to extract the rom before playing it

# Source files definition
SET(SRC_FEX
    7z_C/7zAlloc.c
    7z_C/7zBuf.c
    7z_C/7zCrc.c
    7z_C/7zCrcOpt.c
    7z_C/7zDec.c
    7z_C/7zIn.c
    7z_C/7zStream.c
    7z_C/Bcj2.c
    7z_C/Bra86.c
    7z_C/Bra.c
    7z_C/CpuArch.c
    7z_C/Lzma2Dec.c
    7z_C/LzmaDec.c
    7z_C/Ppmd7.c
    7z_C/Ppmd7Dec.c
    fex/Binary_Extractor.cpp
    fex/blargg_common.cpp
    fex/blargg_errors.cpp
    fex/Data_Reader.cpp
    fex/fex.cpp
    fex/File_Extractor.cpp
    fex/Gzip_Extractor.cpp
    fex/Gzip_Reader.cpp
    fex/Rar_Extractor.cpp
    fex/Zip7_Extractor.cpp
    fex/Zip_Extractor.cpp
    fex/Zlib_Inflater.cpp
)

INCLUDE_DIRECTORIES(
    .
    ${ZLIB_INCLUDE_DIRS}
)

ADD_LIBRARY(
    fex
    STATIC
    ${SRC_FEX}
)

add_dependencies(fex generate)
