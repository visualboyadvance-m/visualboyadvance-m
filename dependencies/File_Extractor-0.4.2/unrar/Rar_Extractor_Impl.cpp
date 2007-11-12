// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#include "rar.hpp"

#include <assert.h>

Rar_Extractor_Impl::Rar_Extractor_Impl( File_Reader* in ) :
	reader( in ),
	arc( this ),
	Unp( this, this ),
	Buffer( this )
{
	first_file = true;
	InitCRC();
}

void rar_out_of_memory( Rar_Error_Handler* eh )
{
	longjmp( eh->jmp_env, 1 );
}

int Rar_Extractor_Impl::UnpRead( byte* out, uint count )
{
	if ( count <= 0 )
		return 0;
	
	if ( count > (uint) UnpPackedSize )
		count = UnpPackedSize;
	
	long result = Read( out, count );
	if ( result >= 0 )
		UnpPackedSize -= result;
	return result;
}

void Rar_Extractor_Impl::UnstoreFile( Int64 DestUnpSize )
{
	Buffer.Alloc( 0x10000 );
	while ( true )
	{
		unsigned int result = UnpRead( &Buffer[0], Buffer.Size() );
		if ( result == 0 || (int) result == -1 )
			break;
		
		result = ((Int64) result < DestUnpSize ? result : int64to32( DestUnpSize ));
		UnpWrite( &Buffer[0], result );
		if ( DestUnpSize >= 0 )
			DestUnpSize -= result;
	}
	Buffer.Reset();
}

const char* Rar_Extractor_Impl::extract( Data_Writer& rar_writer, bool check_crc )
{
	assert( arc.GetHeaderType() == FILE_HEAD );
	
	if ( arc.NewLhd.Flags & (LHD_SPLIT_AFTER | LHD_SPLIT_BEFORE) )
		return "Segmented RAR not supported";
	
	if ( arc.NewLhd.Flags & LHD_PASSWORD )
		return "Encrypted RAR archive not supported";
	
	if ( arc.NewLhd.UnpVer < 13 || arc.NewLhd.UnpVer > UNP_VER )
		return "RAR file uses an unsupported format (too old or too recent)";
	
	Seek( arc.NextBlockPos - arc.NewLhd.FullPackSize );
	
	UnpFileCRC = arc.OldFormat ? 0 : 0xFFFFFFFF;
	PackedCRC = 0xFFFFFFFF;
	UnpPackedSize = arc.NewLhd.FullPackSize;
	MaintainCRC = check_crc;
	this->writer = &rar_writer;
	
	if ( arc.NewLhd.Method == 0x30 )
	{
		UnstoreFile( arc.NewLhd.FullUnpSize );
	}
	else
	{
		Unp.SetDestSize( arc.NewLhd.FullUnpSize );
		if ( arc.NewLhd.UnpVer <= 15 )
			Unp.DoUnpack( 15, arc.Solid && !first_file );
		else
			Unp.DoUnpack( arc.NewLhd.UnpVer, arc.NewLhd.Flags & LHD_SOLID);
	}
	
	first_file = false;
	
	arc.SeekToNext();
	
	if ( check_crc )
	{
		unsigned long correct = arc.NewLhd.FileCRC;
		if ( !arc.OldFormat )
			correct = ~correct;
		
		if ( (UnpFileCRC ^ correct) & 0xFFFFFFFF )
			return "CRC error";
	}
	
	return 0;
}
