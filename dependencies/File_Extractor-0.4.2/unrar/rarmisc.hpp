// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#ifndef _RAR_RARMISC_
#define _RAR_RARMISC_

void InitCRC();
uint CRC( uint crc, void const* in, uint count );
ushort OldCRC( ushort crc, void const* in, uint count );

class BitInput {
public:
	enum { MAX_SIZE = 0x8000 };
protected:
	int InAddr, InBit;
	Rar_Error_Handler* error_handler;
public:
	BitInput( Rar_Error_Handler* eh );
	~BitInput();

	byte* InBuf;

	void InitBitInput()
	{
		InAddr = InBit = 0;
	}
	void addbits( int Bits )
	{
		Bits += InBit;
		InAddr += Bits >> 3;
		InBit = Bits & 7;
	}
	unsigned int getbits()
	{
		unsigned int BitField = (uint) InBuf [InAddr] << 16;
		BitField |= (uint) InBuf [InAddr + 1] << 8;
		BitField |= (uint) InBuf [InAddr + 2];
		BitField >>= (8 - InBit);
		return BitField & 0xFFFF;
	}
	void faddbits( int Bits );
	unsigned int fgetbits();
};

#define MAXWINSIZE      0x400000
#define MAXWINMASK      (MAXWINSIZE-1)

#define LOW_DIST_REP_COUNT 16

#define NC 299  /* alphabet = {0, 1, 2, ..., NC - 1} */
#define DC  60
#define LDC 17
#define RC  28
#define HUFF_TABLE_SIZE (NC+DC+RC+LDC)
#define BC  20

#define NC20 298  /* alphabet = {0, 1, 2, ..., NC - 1} */
#define DC20 48
#define RC20 28
#define BC20 19
#define MC20 257

#endif
