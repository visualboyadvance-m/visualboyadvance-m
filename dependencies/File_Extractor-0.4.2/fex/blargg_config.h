// Library configuration. Modify this file as necessary.

// File_Extractor 0.4.2
#ifndef BLARGG_CONFIG_H
#define BLARGG_CONFIG_H

// Uncomment and edit to support only the listed archive types. See fex_type_list.cpp
// for a list of all types.
//#define FEX_TYPE_LIST fex_7z_type,fex_gz_type,fex_rar_type,fex_zip_type,fex_single_file_type

// Uncomment to enable RAR archive support. Doing so adds extra licensing restrictions
// to this library (see unrar/readme.txt for more information).
//#define FEX_ENABLE_RAR 1

// Uncomment if you get errors in the bool section of blargg_common.h
//#define BLARGG_COMPILER_HAS_BOOL 1

// Use standard config.h if present
#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#endif
