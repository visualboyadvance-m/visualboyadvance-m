#ifndef _RAR_ARCHIVE_
#define _RAR_ARCHIVE_

typedef ComprDataIO File;
#include "rawread.hpp"

enum RARFORMAT {RARFMT_NONE,RARFMT14,RARFMT15,RARFMT50,RARFMT_FUTURE};

class Archive:public File
{
private:
    void ConvertFileHeader(FileHeader *hd);
    void WriteBlock50(HEADER_TYPE HeaderType,BaseBlock *wb,bool OnlySetSize,bool NonFinalWrite);
    unrar_err_t ReadHeader14(size_t *ReadSize);
    unrar_err_t ReadHeader15(size_t *ReadSize);
    unrar_err_t ReadHeader50(size_t *ReadSize);
    unrar_err_t ProcessExtra50(RawRead *Raw,size_t ExtraSize,BaseBlock *bb);

	RawRead Raw;

    HEADER_TYPE CurHeaderType;
    
public:
	Archive();
    RARFORMAT IsSignature(const byte *D,size_t Size);
	unrar_err_t IsArchive();
    size_t SearchBlock(HEADER_TYPE HeaderType);
    size_t SearchSubBlock(const wchar *Type);
    size_t SearchRR();
    unrar_err_t ReadHeader(size_t *ReadSize);
	void SeekToNext();
    bool IsArcDir();
    bool IsArcLabel();
    int64 GetStartPos();
	HEADER_TYPE GetHeaderType() {return(CurHeaderType);};

	BaseBlock ShortBlock;
    MarkHeader MarkHead;
    MainHeader MainHead;
    FileHeader FileHead;
    EndArcHeader EndArcHead;
    SubBlockHeader SubBlockHead;
	FileHeader SubHead;
    ProtectHeader ProtectHead;

    int64 CurBlockPos;
    int64 NextBlockPos;

    RARFORMAT Format;
	bool Solid;
	enum { SFXSize = 0 }; // self-extracting not supported
	ushort HeaderCRC;
};

#endif
