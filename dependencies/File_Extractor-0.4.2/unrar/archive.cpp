// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#include "rar.hpp"

#include <assert.h>

#ifndef check
	#define check( expr ) ((void) 0)
#endif

Archive::Archive( Rar_Extractor_Impl* impl_ ) : NewLhd( impl_ ), SubHead( impl_ ), Raw( impl_ )
{
	impl = impl_;
	OldFormat=false;
	Solid=false;
	LastReadBlock=0;

	CurBlockPos=0;
	NextBlockPos=0;

	memset(&NewMhd,0,sizeof(NewMhd));
	NewMhd.HeadType=MAIN_HEAD;
	NewMhd.HeadSize=SIZEOF_NEWMHD;
	HeaderCRC=0;
}

bool Archive::IsSignature(byte *D)
{
	OldFormat = false;
	if ( D [0] != 0x52 )
		return false;
	
	if ( D [1] == 0x45 && D [2] == 0x7E && D [3] == 0x5E )
	{
		OldFormat = true;
		return true;
	}
	
	if ( D [1] == 0x61 && D [2] == 0x72 && D [3] == 0x21 &&
			D [4] == 0x1A && D [5] == 0x07 && D [6] == 0x00)
		return true;
	
	return false;
}

int Archive::IsArchive()
{
	if ( impl->Read( MarkHead.Mark,SIZEOF_MARKHEAD ) != SIZEOF_MARKHEAD )
		return 0;
	
	if ( !IsSignature( MarkHead.Mark ) )
		return 0;
	
	return 1;
}

const char* Archive::IsArchive2()
{
	if (OldFormat)
		impl->Seek(0);
	
	const char* error = ReadHeader();
	if ( error )
		return error;
	
	SeekToNext();
	if ( OldFormat )
	{
		NewMhd.Flags = OldMhd.Flags & 0x3f;
		NewMhd.HeadSize = OldMhd.HeadSize;
	}
	else if ( HeaderCRC != NewMhd.HeadCRC )
	{
		return "RAR header CRC is wrong";
	}
	
	if ( NewMhd.Flags & MHD_VOLUME )
		return "Split RAR files are not supported";
	
	if ( NewMhd.Flags & MHD_PASSWORD )
		return "Encrypted RAR files are not supported";
	
	Solid = (NewMhd.Flags & MHD_SOLID) != 0;
	
	return NULL;
}

void Archive::SeekToNext()
{
	impl->Seek( NextBlockPos );
}

// RawRead

RawRead::RawRead( Rar_Extractor_Impl* impl_ ) : Data( impl_ )
{
	impl = impl_;
	ReadPos = 0;
	DataSize = 0;
}

void RawRead::Reset()
{
	check( ReadPos == DataSize );
	ReadPos = 0;
	DataSize = 0;
	Data.Reset();
}

void RawRead::Read( int count )
{
	if ( count )
	{
		Data.Alloc( DataSize + count );
		DataSize += impl->Read( &Data [DataSize], count );
	}
}

void RawRead::get8( byte& out )
{
	out = Data [ReadPos];
	ReadPos++;
}

void RawRead::get16( ushort& out )
{
	out = Data [ReadPos] + (Data [ReadPos + 1] << 8);
	ReadPos += 2;
}

void RawRead::get32( uint& out )
{
	out=Data[ReadPos]+(Data[ReadPos+1]<<8)+(Data[ReadPos+2]<<16)+
			(Data[ReadPos+3]<<24);
	ReadPos+=4;
}

void RawRead::Get( byte* out, int count )
{
	memcpy( out, &Data [ReadPos], count );
	ReadPos += count;
}

uint RawRead::GetCRC( bool partial )
{
	uint crc = 0xFFFFFFFF;
	if ( DataSize > 2 )
		crc = CRC( crc, &Data[2], (partial ? ReadPos : DataSize) - 2 );
	return ~crc & 0xFFFF;
}

// Archive::ReadHeader()

const char* Archive::ReadHeader()
{
	CurBlockPos = impl->Tell();

	if ( OldFormat )
		return ReadOldHeader();

	Raw.Reset();

	Raw.Read( SIZEOF_SHORTBLOCKHEAD );
	if ( Raw.Size() == 0 )
	{
		Int64 ArcSize = impl->Size();
		if ( CurBlockPos > ArcSize || NextBlockPos > ArcSize )
			return "RAR file is missing data at the end";
		return end_of_rar;
	}
	check( Raw.Size() == SIZEOF_SHORTBLOCKHEAD );

	Raw.get16( ShortBlock.HeadCRC );
	byte HeadType;
	Raw.get8( HeadType );
	ShortBlock.HeadType=(HEADER_TYPE)HeadType;
	Raw.get16( ShortBlock.Flags );
	Raw.get16( ShortBlock.HeadSize );
	if ( ShortBlock.HeadSize<SIZEOF_SHORTBLOCKHEAD)
		return "CRC error in RAR file";

	if ( ShortBlock.HeadType==COMM_HEAD)
		Raw.Read( SIZEOF_COMMHEAD-SIZEOF_SHORTBLOCKHEAD );
	else
		if ( ShortBlock.HeadType==MAIN_HEAD && (ShortBlock.Flags & MHD_COMMENT)!=0)
			Raw.Read( SIZEOF_NEWMHD-SIZEOF_SHORTBLOCKHEAD );
		else
			Raw.Read( ShortBlock.HeadSize-SIZEOF_SHORTBLOCKHEAD );

	NextBlockPos = CurBlockPos + ShortBlock.HeadSize;

	switch ( ShortBlock.HeadType )
	{
		case MAIN_HEAD:
			*(BaseBlock*)&NewMhd=ShortBlock;
			Raw.get16( NewMhd.HighPosAV );
			Raw.get32( NewMhd.PosAV );
			break;
		case ENDARC_HEAD:
			// ignore
			break;
		
		case FILE_HEAD:
		case NEWSUB_HEAD:
			{
				FileHeader *hd=ShortBlock.HeadType==FILE_HEAD ? &NewLhd:&SubHead;
				*(BaseBlock*)hd=ShortBlock;
				Raw.get32( hd->PackSize );
				Raw.get32( hd->UnpSize );
				Raw.get8( hd->HostOS );
				Raw.get32( hd->FileCRC );
				Raw.get32( hd->FileTime );
				Raw.get8( hd->UnpVer );
				Raw.get8( hd->Method );
				Raw.get16( hd->NameSize );
				Raw.get32( hd->FileAttr );
				
				hd->HighPackSize=hd->HighUnpSize=0;
				if (hd->Flags & LHD_LARGE)
				{
					Raw.get32( hd->HighPackSize );
					Raw.get32( hd->HighUnpSize );
				}
				
				assert( hd->UnpSize != 0xFFFFFFFF );
				
				if ( hd->HighPackSize || hd->HighUnpSize )
					return "Huge RAR archives not supported";
				
				hd->FullPackSize = hd->PackSize;
				hd->FullUnpSize  = hd->UnpSize;

				char FileName[NM*4];
				int NameSize=Min(hd->NameSize,sizeof(FileName)-1 );
				Raw.Get((byte*)FileName,NameSize );
				FileName[NameSize]=0;

				strncpy(hd->FileName,FileName,sizeof(hd->FileName) );
				hd->FileName[sizeof(hd->FileName)-1]=0;

				if (hd->HeadType==NEWSUB_HEAD)
				{
					int DataSize=hd->HeadSize-hd->NameSize-SIZEOF_NEWLHD;
					if (hd->Flags & LHD_SALT)
						DataSize-=SALT_SIZE;
					if (DataSize>0)
					{
						hd->SubData.Alloc(DataSize );
						Raw.Get(&hd->SubData[0],DataSize );
						//if ( !strcmp( hd->FileName, SUBHEAD_TYPE_RR ) )
						//byte *D=&hd->SubData[8];
					}
				}
				else if (hd->HeadType==FILE_HEAD)
				{
					hd->Flags &= ~LHD_UNICODE;
					
					if (NewLhd.UnpVer<20 && (NewLhd.FileAttr & 0x10))
						NewLhd.Flags|=LHD_DIRECTORY;
					
					if (NewLhd.HostOS>=HOST_MAX)
					{
						if ((NewLhd.Flags & LHD_WINDOWMASK)==LHD_DIRECTORY)
							NewLhd.FileAttr=0x10;
						else
							NewLhd.FileAttr=0x20;
					}
				}
				
				if (hd->Flags & LHD_SALT)
					Raw.Get(hd->Salt,SALT_SIZE );
				
				if (hd->Flags & LHD_EXTTIME)
				{
					ushort Flags;
					Raw.get16( Flags );
					for (int I=0;I<4;I++)
					{
						uint rmode=Flags>>(3-I)*4;
						if ((rmode & 8)==0)
							continue;
						if (I!=0)
						{
							uint DosTime;
							Raw.get32( DosTime );
						}
						
						// skip time info
						for ( int n = rmode & 3; n--; )
						{
							byte CurByte;
							Raw.get8( CurByte );
						}
					}
				}
				
				NextBlockPos += hd->FullPackSize;
				HeaderCRC = Raw.GetCRC( hd->Flags & LHD_COMMENT );
				if ( hd->HeadCRC != HeaderCRC )
					return "RAR file header CRC is incorrect";
			}
			break;
		default:
			if (ShortBlock.Flags & LONG_BLOCK)
			{
				uint DataSize;
				Raw.get32( DataSize );
				NextBlockPos += DataSize;
			}
			break;
	}
	
	HeaderCRC = Raw.GetCRC( false );
	CurHeaderType = ShortBlock.HeadType;
	
	if ( NextBlockPos <= CurBlockPos )
		return "CRC error in RAR block header";
	
	return Raw.Size() ? NULL : end_of_rar;
}

const char* Archive::ReadOldHeader()
{
	Raw.Reset();
	if (CurBlockPos<=SFXSize)
	{
		Raw.Read(SIZEOF_OLDMHD );
		Raw.Get(OldMhd.Mark,4 );
		Raw.get16( OldMhd.HeadSize );
		Raw.get8( OldMhd.Flags );
		NextBlockPos=CurBlockPos+OldMhd.HeadSize;
		CurHeaderType=MAIN_HEAD;
	}
	else
	{
		OldFileHeader OldLhd;
		Raw.Read(SIZEOF_OLDLHD );
		NewLhd.HeadType=FILE_HEAD;
		Raw.get32( NewLhd.PackSize );
		Raw.get32( NewLhd.UnpSize );
		Raw.get16( OldLhd.FileCRC );
		Raw.get16( NewLhd.HeadSize );
		Raw.get32( NewLhd.FileTime );
		Raw.get8( OldLhd.FileAttr );
		Raw.get8( OldLhd.Flags );
		Raw.get8( OldLhd.UnpVer );
		Raw.get8( OldLhd.NameSize );
		Raw.get8( OldLhd.Method );

		NewLhd.Flags=OldLhd.Flags|LONG_BLOCK;
		NewLhd.UnpVer=(OldLhd.UnpVer==2) ? 13 : 10;
		NewLhd.Method=OldLhd.Method+0x30;
		NewLhd.NameSize=OldLhd.NameSize;
		NewLhd.FileAttr=OldLhd.FileAttr;
		NewLhd.FileCRC=OldLhd.FileCRC;
		NewLhd.FullPackSize=NewLhd.PackSize;
		NewLhd.FullUnpSize=NewLhd.UnpSize;

		Raw.Read(OldLhd.NameSize );
		Raw.Get((byte*)NewLhd.FileName,OldLhd.NameSize );
		NewLhd.FileName[OldLhd.NameSize]=0;

		if (Raw.Size()!=0)
			NextBlockPos=CurBlockPos+NewLhd.HeadSize+NewLhd.PackSize;
		CurHeaderType=FILE_HEAD;
	}
	return (NextBlockPos > CurBlockPos && Raw.Size()) ? NULL : end_of_rar;
}
