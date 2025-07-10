#include <stdio.h>
#include "rar.hpp"

#include "unrar.h"

#define DataIO Arc

unrar_err_t CmdExtract::ExtractCurrentFile( bool SkipSolid, bool check_compatibility_only )
{
	check( Arc.GetHeaderType() == FILE_HEAD );

	if ( Arc.FileHead.SplitBefore || Arc.FileHead.SplitAfter )
		return unrar_err_segmented;

	if ( Arc.FileHead.Encrypted )
		return unrar_err_encrypted;
	
	if ( !check_compatibility_only )
	{
		check(   Arc.NextBlockPos-Arc.FileHead.PackSize == Arc.Tell() );
		Arc.Seek(Arc.NextBlockPos-Arc.FileHead.PackSize,SEEK_SET);
	}
	
	// (removed lots of command-line handling)

#ifdef SFX_MODULE
	if ((Arc.FileHead.UnpVer!=UNP_VER && Arc.FileHead.UnpVer!=29) &&
			Arc.FileHead.Method!=0x30)
#else
	if (Arc.FileHead.UnpVer!=VER_UNPACK5 &&
        (Arc.FileHead.UnpVer<13 || Arc.FileHead.UnpVer>VER_UNPACK))
#endif
	{
		if (Arc.FileHead.UnpVer>VER_UNPACK)
			return unrar_err_new_algo;
		return unrar_err_old_algo;
	}
	
	if ( check_compatibility_only )
		return unrar_ok;
	
	// (removed lots of command-line/encryption/volume handling)
	
	update_first_file_pos();
	FileCount++;
	DataIO.UnpFileCRC=Arc.OldFormat ? 0 : 0xffffffff;
	// (removed decryption)
    DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,1);
    DataIO.PackedDataHash.Init(Arc.FileHead.FileHash.Type,1);
    DataIO.SetPackedSizeToRead(Arc.FileHead.PackSize);
    DataIO.SetSkipUnpCRC(SkipSolid);
	// (removed command-line handling)
	DataIO.SetSkipUnpCRC(SkipSolid);

	if (Arc.FileHead.Method==0)
		UnstoreFile(Arc.FileHead.UnpSize);
	else
	{
		// Defer creation of Unpack until first extraction
		if ( !Unp )
		{
			Unp = new Unpack( &Arc );
			if ( !Unp )
				return unrar_err_memory;
		}
		
        Unp->Init(Arc.FileHead.WinSize,Arc.FileHead.Solid);
        Unp->SetDestSize(Arc.FileHead.UnpSize);
#ifndef SFX_MODULE
		if (Arc.Format!=RARFMT50 && Arc.FileHead.UnpVer<=15)
			Unp->DoUnpack(15,FileCount>1 && Arc.Solid);
		else
#endif
			Unp->DoUnpack(Arc.FileHead.UnpVer,Arc.FileHead.Solid);
	}
	
	// (no need to seek to next file)
	
	if (!SkipSolid)
	{
        HashValue UnpHash;
        DataIO.UnpHash.Result(&UnpHash);
	    if (UnpHash==Arc.FileHead.FileHash)
        {
	    	// CRC is correct
	    }
	    else
	    {
			return unrar_err_corrupt;
	    }
	}
	
	// (removed broken file handling)
	// (removed command-line handling)

	return unrar_ok;
}


void CmdExtract::UnstoreFile(int64 DestUnpSize)
{
	Buffer.Alloc((int)Min(DestUnpSize,0x10000));
	while (1)
	{
		unsigned int Code=DataIO.UnpRead(&Buffer[0],(uint)Buffer.Size());
		if (Code==0 || (int)Code==-1)
			break;
		Code=Code<DestUnpSize ? Code:int64to32(DestUnpSize);
		DataIO.UnpWrite(&Buffer[0],Code);
		if (DestUnpSize>=0)
			 DestUnpSize-=Code;
	}
	Buffer.Reset();
}
