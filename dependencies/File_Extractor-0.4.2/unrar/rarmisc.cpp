// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#include "rar.hpp"

// CRC

static uint CRCTab [256];

void InitCRC()
{
	if ( !CRCTab [1] )
	{
		for ( int i = 256; --i >= 0; )
		{
			unsigned c = i;
			for ( int n = 8; --n >= 0; )
				c = (c >> 1) ^ (0xEDB88320L & -(c & 1));
			CRCTab [i] = c;
		}
	}
}

uint CRC( uint crc, void const* in, uint count )
{
	byte const* p   = (byte const*) in;
	byte const* end = (byte const*) in + count;
	while ( p < end )
		crc = (crc >> 8) ^ CRCTab [(crc & 0xFF) ^ *p++];
	return crc;
}

ushort OldCRC( ushort crc, void const* in, uint count )
{
	byte const* p   = (byte const*) in;
	byte const* end = p + count;
	while ( p < end )
	{
		crc += *p++;
		crc = (crc << 1) | (crc >> 15 & 1);
	}
	return crc & 0xFFFF;
}

// BitInput

BitInput::BitInput( Rar_Error_Handler* eh )
{
	error_handler = eh;
	InBuf = (byte*) malloc( MAX_SIZE );
	if ( !InBuf )
		rar_out_of_memory( error_handler );
}

BitInput::~BitInput()
{
	free( InBuf );
}

void BitInput::faddbits(int Bits)
{
	addbits( Bits );
}

unsigned int BitInput::fgetbits()
{
	return getbits();
}
