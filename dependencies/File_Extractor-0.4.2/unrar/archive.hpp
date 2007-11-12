// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#ifndef _RAR_ARCHIVE_
#define _RAR_ARCHIVE_

class RawRead {
private:
	Rar_Array<byte> Data;
	Rar_Extractor_Impl* impl;
	int DataSize;
	int ReadPos;
public:
	RawRead( Rar_Extractor_Impl* );
	~RawRead() { Reset(); }
	void Reset();
	void Read(int Size);
	void get8( byte& out );
	void get16( ushort& out );
	void get32( uint& out );
	void Get(byte *Field,int Size);
	uint GetCRC( bool partial );
	int Size() {return DataSize;}
};

class Archive {
public:

	Archive( Rar_Extractor_Impl* );
	int IsArchive();
	const char* IsArchive2();
	const char* ReadHeader();
	void SeekToNext();
	int GetHeaderType() {return(CurHeaderType);};

	BaseBlock ShortBlock;
	MainHeader NewMhd;
	FileHeader NewLhd;
	FileHeader SubHead;
	
	Int64 CurBlockPos;
	Int64 NextBlockPos;

	bool OldFormat;
	bool Solid;
	enum { SFXSize = 0 }; // we don't support self-extracting archive data at the beginning
	ushort HeaderCRC;

private:
	RawRead Raw;
	Rar_Error_Handler* impl;
	
	MarkHeader MarkHead;
	OldMainHeader OldMhd;

	int LastReadBlock;
	int CurHeaderType;

	bool IsSignature(byte *D);
	const char* ReadOldHeader();
};

#endif
