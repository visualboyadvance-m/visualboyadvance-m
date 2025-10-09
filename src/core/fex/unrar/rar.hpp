// This source code is a heavily modified version based on the unrar package.
// It may NOT be used to develop a RAR (WinRAR) compatible archiver.
// See license.txt for copyright and licensing.

// unrar_core 5.1.7
#ifndef RAR_COMMON_HPP
#define RAR_COMMON_HPP

#include "unrar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>

//// Glue

// One goal is to keep source code as close to original as possible, so
// that changes to the original can be found and merged more easily.

// These names are too generic and might clash (or have already, hmpf)
#define Array               Rar_Array
#define uint32              rar_uint32
#define int32               rar_int32
#define Unpack              Rar_Unpack
#define Archive             Rar_Archive
#define RawRead             Rar_RawRead
#define BitInput            Rar_BitInput
#define ModelPPM            Rar_ModelPPM
#define RangeCoder          Rar_RangeCoder
#define SubAllocator        Rar_SubAllocator
#define UnpackFilter        Rar_UnpackFilter
#define VM_PreparedProgram  Rar_VM_PreparedProgram
#define CRCTab              Rar_CRCTab

// original source used rar* names for these as well
#define rarmalloc   malloc
#define rarrealloc  realloc
#define rarfree     free

// Internal flags, possibly set later
#undef SFX_MODULE
#undef VM_OPTIMIZE
#undef VM_STANDARDFILTERS
#undef NORARVM

// During debugging if expr is false, prints message then continues execution
#ifndef check
	#define check( expr ) ((void) 0)
#endif

struct Rar_Error_Handler
{
	jmp_buf jmp_env;
	
	void MemoryError();
	void ReportError( unrar_err_t );
};

// throw spec is mandatory in ISO C++ if operator new can return NULL
#if __cplusplus >= 199711 || __GNUC__ >= 3
	#define UNRAR_NOTHROW throw ()
#else
	#define UNRAR_NOTHROW
#endif

struct Rar_Allocator
{
	// provides allocator that doesn't throw an exception on failure
	static void operator delete ( void* p ) { free( p ); }
	static void* operator new ( size_t s ) UNRAR_NOTHROW { return malloc( s ); }
	static void* operator new ( size_t, void* p ) UNRAR_NOTHROW { return p; }
};

//// os.hpp
#define FALSE 0
#define TRUE  1
#undef STRICT_ALIGNMENT_REQUIRED
#undef LITTLE_ENDIAN
#define  NM  1024

#if defined (__i386__) || defined (__x86_64__) || defined (_M_IX86) || defined (_M_X64)
	// Optimizations mostly only apply to x86
	#define LITTLE_ENDIAN
	#define ALLOW_NOT_ALIGNED_INT
#endif

#if defined(__sparc) || defined(sparc) || defined(__sparcv9)
/* prohibit not aligned access to data structures in text comression
   algorithm, increases memory requirements */
	#define STRICT_ALIGNMENT_REQUIRED
#endif

//// rartypes.hpp
#if INT_MAX == 0x7FFFFFFF && UINT_MAX == 0xFFFFFFFF
	typedef unsigned int     uint32; //32 bits exactly
	typedef          int     int32;  //signed 32 bits exactly
	#define PRESENT_INT32
#endif

typedef unsigned char    byte;   //8 bits
typedef unsigned short   ushort; //preferably 16 bits, but can be more
typedef unsigned int     uint;   //32 bits or more

typedef wchar_t wchar;

#define GET_SHORT16(x) (sizeof(ushort)==2 ? (ushort)(x):((x)&0xffff))
#define GET_UINT32(x)  (sizeof(uint  )==4 ? (uint  )(x):((x)&0xffffffff))

//// rardefs.hpp
#define  Min(x,y) (((x)<(y)) ? (x):(y))
#define  Max(x,y) (((x)>(y)) ? (x):(y))
#define  ALIGN_VALUE(v,a) (size_t(v) + ( (~size_t(v) + 1) & (a - 1) ) )
#define  ASIZE(x) (sizeof(x)/sizeof(x[0]))

//// int64.hpp
typedef unrar_long_long int64;
typedef unrar_ulong_long uint64;

#define int64to32(x) ((uint)(x))
#define int32to64(high,low) ((((int64)(high))<<31<<1)+(low))
#define is64plus(x) (x>=0)

#define INT64MAX int32to64(0x7fffffff,0)
#define INT64NDF int32to64(0x7fffffff,0x7fffffff)

//// crc.hpp
extern uint crc_tables[8][256];
void InitCRCTables();
uint CRC32(uint StartCRC,const void *Addr,size_t Size);
ushort Checksum14(ushort StartCRC,const void *Addr,size_t Size);

#define SHA256_DIGEST_SIZE 32
#include "blake2s.hpp"
#include "hash.hpp"

//// rdwrfn.hpp
class ComprDataIO
		: public Rar_Error_Handler
{
public:
	unrar_read_func  user_read;
	unrar_write_func user_write;
	void* user_read_data;
	void* user_write_data;
	unrar_err_t write_error; // once write error occurs, no more writes are made
 	int64 Tell_;
	bool OldFormat;

private:
	int64 UnpPackedSize;
    bool SkipUnpCRC;

public:
	int UnpRead(byte *Addr,uint Count);
    void UnpWrite(byte *Addr,uint Count);
	void SetSkipUnpCRC( bool b ) { SkipUnpCRC = b; }
	void SetPackedSizeToRead( int64 n ) { UnpPackedSize = n; }

	uint UnpFileCRC;

	void Seek(int64 Offset, int Method = 0 ) { (void)Method; Tell_ = Offset; }
	int64 Tell() { return Tell_; }
	int Read( void* p, int n );
    
    DataHash PackedDataHash; // Packed write and unpack read hash.
    DataHash PackHash; // Pack read hash.
    DataHash UnpHash;  // Unpack write hash.
};

//// secpassword.hpp
void cleandata(void *data,size_t size);

//// pathfn.hpp
wchar* GetWideName(const char *Name,const wchar *NameW,wchar *DestW,size_t DestSize);
void UnixSlashToDos(const char *SrcName, char *DestName, size_t MaxLength);
void DosSlashToUnix(const char *SrcName, char *DestName, size_t MaxLength);
void UnixSlashToDos(const wchar *SrcName, wchar *DestName, size_t MaxLength);
void DosSlashToUnix(const wchar *SrcName, wchar *DestName, size_t MaxLength);

//// rar.hpp
class Unpack;
#include "array.hpp"
#include "unicode.hpp"
#include "timefn.hpp"
#include "headers.hpp"
#include "headers5.hpp"
#include "getbits.hpp"
#include "archive.hpp"
#include "rawread.hpp"
#include "encname.hpp"
#include "compress.hpp"
#include "rarvm.hpp"
#include "model.hpp"
#include "strfn.hpp"
#include "unpack.hpp"

//// savepos.hpp
class SaveFilePos
{
private:
    File *SaveFile;
    int64 SavePos;
public:
    SaveFilePos(File &Src)
    {
        SaveFile=&Src;
        SavePos=Src.Tell();
    }
    ~SaveFilePos()
    {
        SaveFile->Seek(SavePos,SEEK_SET);
    }
};

//// extract.hpp
/** RAR archive */
struct unrar_t
	: public Rar_Allocator
{
	unrar_info_t info;
 	unrar_pos_t begin_pos;
	unrar_pos_t solid_pos;
	unrar_pos_t first_file_pos;
	void const* data_;
	void* own_data_;
	void (*close_file)( void* ); // func ptr to avoid linking fclose() in unnecessarily
	bool done;
	long FileCount;
 	Unpack* Unp;
	Array<byte> Buffer;
	// large items last
	Archive Arc;
 	
	unrar_t();
	~unrar_t();
	void UnstoreFile( int64 );
	unrar_err_t ExtractCurrentFile( bool SkipSolid = false, bool check_compatibility_only = false );
	void update_first_file_pos()
	{
		if ( FileCount == 0 )
			first_file_pos = Arc.CurBlockPos;
	}
};

typedef unrar_t CmdExtract;

#endif
