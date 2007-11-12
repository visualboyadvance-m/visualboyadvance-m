// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#ifndef RAR_EXTRACTOR_IMPL_H
#define RAR_EXTRACTOR_IMPL_H

#include "rar.hpp"

extern const char end_of_rar [];

class Data_Writer;
class File_Reader;

class Rar_Extractor_Impl : public Rar_Allocator {
public:
	void Seek( long );
	long Tell();
	long Read( void*, long );
	long Size();
	
	int UnpRead( byte*, uint );
	void UnpWrite( byte const*, uint );
	jmp_buf jmp_env;
	
private:
	File_Reader* reader;
	Archive arc;
	bool first_file;
	Unpack Unp;
 	Rar_Array<byte> Buffer;
	const char* write_error; // once write error occurs, no more writes are made
	Data_Writer* writer;
	Int64 UnpPackedSize;
	uint UnpFileCRC,PackedCRC;
	bool MaintainCRC;
	
	const char* extract( Data_Writer&, bool check_crc = 1 );
	
	Rar_Extractor_Impl( File_Reader* in );
	void UnstoreFile( Int64 );
	
	friend class Rar_Extractor;
};

#endif
