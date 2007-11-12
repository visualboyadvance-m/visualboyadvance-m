// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#ifndef RAR_COMMON_HPP
#define RAR_COMMON_HPP

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

const int NM = 1024; // max filename length

typedef unsigned char  byte;
typedef unsigned short ushort;
typedef unsigned int   uint;

#define Min( x, y ) ((x) < (y) ? (x) : (y))

// 64-bit int support disabled, since this library isn't meant for huge 2GB+ archives
typedef long Int64;
#define int64to32(x) ((uint)(x))

#ifdef __GNUC__
	#define _PACK_ATTR __attribute__ ((packed))
#else
	#define _PACK_ATTR
#endif

// used as base class to override new and delete to not throw exceptions
struct Rar_Allocator
{
// throw spec mandatory in ISO C++ if operator new can return NULL
#if __cplusplus >= 199711 || __GNUC__ >= 3
	void* operator new ( size_t s ) throw () { return malloc( s ); }
#else
	void* operator new ( size_t s )          { return malloc( s ); }
#endif
	void operator delete ( void* p ) { free( p ); }
};

class Rar_Extractor_Impl;
typedef Rar_Extractor_Impl Rar_Error_Handler;
void rar_out_of_memory( Rar_Error_Handler* );

// lots of order dependencies here
class Unpack;
#include "rarmisc.hpp"
#include "array.hpp"
#include "headers.hpp"
#include "archive.hpp"
#include "rarvm.hpp"
#include "suballoc.hpp"
#include "model.hpp"
#include "unpack.hpp"
#include "Rar_Extractor_Impl.h"

#endif
