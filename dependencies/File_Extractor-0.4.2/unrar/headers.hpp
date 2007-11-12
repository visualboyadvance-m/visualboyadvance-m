// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#ifndef _RAR_HEADERS_
#define _RAR_HEADERS_

#define  SIZEOF_MARKHEAD         7
#define  SIZEOF_OLDMHD           7
#define  SIZEOF_NEWMHD          13
#define  SIZEOF_OLDLHD          21
#define  SIZEOF_NEWLHD          32
#define  SIZEOF_SHORTBLOCKHEAD   7
#define  SIZEOF_SUBBLOCKHEAD    14
#define  SIZEOF_COMMHEAD        13

#define  UNP_VER                29

#define  MHD_VOLUME         0x0001
#define  MHD_COMMENT        0x0002
#define  MHD_SOLID          0x0008
#define  MHD_PASSWORD       0x0080

#define  LHD_SPLIT_BEFORE   0x0001
#define  LHD_SPLIT_AFTER    0x0002
#define  LHD_PASSWORD       0x0004
#define  LHD_COMMENT        0x0008
#define  LHD_SOLID          0x0010

#define  LHD_WINDOWMASK     0x00e0
#define  LHD_DIRECTORY      0x00e0

#define  LHD_LARGE          0x0100
#define  LHD_UNICODE        0x0200
#define  LHD_SALT           0x0400
#define  LHD_EXTTIME        0x1000

#define  LONG_BLOCK         0x8000

enum HEADER_TYPE {
	MARK_HEAD=0x72,MAIN_HEAD=0x73,FILE_HEAD=0x74,COMM_HEAD=0x75,AV_HEAD=0x76,
	SUB_HEAD=0x77,PROTECT_HEAD=0x78,SIGN_HEAD=0x79,NEWSUB_HEAD=0x7a,
	ENDARC_HEAD=0x7b
};

enum HOST_SYSTEM {
	HOST_MSDOS=0,HOST_OS2=1,HOST_WIN32=2,HOST_UNIX=3,HOST_MACOS=4,
	HOST_BEOS=5,HOST_MAX
};

struct OldMainHeader
{
	byte Mark[4];
	ushort HeadSize;
	byte Flags;
};

struct OldFileHeader
{
	uint PackSize;
	uint UnpSize;
	ushort FileCRC;
	ushort HeadSize;
	uint FileTime;
	byte FileAttr;
	byte Flags;
	byte UnpVer;
	byte NameSize;
	byte Method;
};

struct MarkHeader
{
	byte Mark[7];
};

struct BaseBlock
{
	ushort HeadCRC;
	HEADER_TYPE HeadType;//byte
	ushort Flags;
	ushort HeadSize;
};

struct BlockHeader:BaseBlock
{
	union {
		uint DataSize;
		uint PackSize;
	};
};

struct MainHeader:BlockHeader
{
	ushort HighPosAV;
	uint PosAV;
};

#define SALT_SIZE     8

struct FileHeader:BlockHeader
{
	uint UnpSize;
	byte HostOS;
	uint FileCRC;
	uint FileTime;
	byte UnpVer;
	byte Method;
	ushort NameSize;
	union {
		uint FileAttr;
		uint SubFlags;
	};
	uint HighPackSize;
	uint HighUnpSize;
	char FileName[NM];
	Rar_Array<byte> SubData;
	byte Salt[SALT_SIZE];
	
	Int64 FullPackSize;
	Int64 FullUnpSize;
	
	FileHeader( Rar_Error_Handler* eh ) : SubData( eh ) { }
};

#endif
