#include <stdio.h>
#include "rar.hpp"

#include "unrar.h"

Archive::Archive() : Raw( this )
{
    Format=RARFMT15;
	Solid=false;

	CurBlockPos=0;
	NextBlockPos=0;

    memset(&MainHead,0,sizeof(MainHead));
    memset(&EndArcHead,0,sizeof(EndArcHead));

    HeaderCRC=0;
}

RARFORMAT Archive::IsSignature(const byte *D,size_t Size)
{
    RARFORMAT Type=RARFMT_NONE;
    if (Size>=1 && D[0]==0x52) {
#ifndef SFX_MODULE
        if (Size>=4 && D[1]==0x45 && D[2]==0x7e && D[3]==0x5e)
            Type=RARFMT14;
        else {
#endif
            if (Size>=7 && D[1]==0x61 && D[2]==0x72 && D[3]==0x21 && D[4]==0x1a && D[5]==0x07)
            {
                // We check for non-zero last signature byte, so we can return
                // a sensible warning in case we'll want to change the archive
                // format sometimes in the future.
                if (D[6]==0)
                    Type=RARFMT15;
                else if (D[6]==1)
                    Type=RARFMT50;
                else if (D[6]==2)
                    Type=RARFMT_FUTURE;
            }
#ifndef SFX_MODULE
        }
#endif
    }
	return Type;
}


unrar_err_t Archive::IsArchive()
{
	if (Read(MarkHead.Mark,SIZEOF_MARKHEAD3)!=SIZEOF_MARKHEAD3)
		return unrar_err_not_arc;

    RARFORMAT Type;
	if ((Type=IsSignature(MarkHead.Mark,SIZEOF_MARKHEAD3))!=RARFMT_NONE)
	{
        Format=Type;
		if (Format==RARFMT14)
			Seek(Tell()-SIZEOF_MARKHEAD3,SEEK_SET);
	}
	else
	{
		if (SFXSize==0)
			return unrar_err_not_arc;
	}
    if (Format==RARFMT_FUTURE)
        return unrar_err_new_algo;
    if (Format==RARFMT50) // RAR 5.0 signature is one byte longer.
    {
        Read(MarkHead.Mark+SIZEOF_MARKHEAD3,1);
        if (MarkHead.Mark[SIZEOF_MARKHEAD3]!=0)
            return unrar_err_not_arc;
        MarkHead.HeadSize=SIZEOF_MARKHEAD5;
    }
    else
        MarkHead.HeadSize=SIZEOF_MARKHEAD3;

    unrar_err_t error;
    size_t HeaderSize;
    while ((error=ReadHeader(&HeaderSize))==unrar_ok && HeaderSize!=0)
    {
        HEADER_TYPE Type=GetHeaderType();
        // In RAR 5.0 we need to quit after reading HEAD_CRYPT if we wish to
        // avoid the password prompt.
        if (Type==HEAD_MAIN)
            break;
        SeekToNext();
    }
	if ( error != unrar_ok )
		return error;

    SeekToNext();
    
    return unrar_ok;
}

void Archive::SeekToNext()
{
	Seek(NextBlockPos,SEEK_SET);
}
