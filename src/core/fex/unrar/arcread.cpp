#include "rar.hpp"

#include "unrar.h"
#include "unicode.hpp"
#include "encname.hpp"

// arcread.cpp
unrar_err_t Archive::ReadHeader(size_t * ReadSize_)
{
	CurBlockPos=Tell();
    
    unrar_err_t Error;
    size_t ReadSize;
    switch(Format)
    {
#ifndef SFX_MODULE
        case RARFMT14:
            Error=ReadHeader14(&ReadSize);
            break;
#endif
        case RARFMT15:
            Error=ReadHeader15(&ReadSize);
            break;
        case RARFMT50:
            Error=ReadHeader50(&ReadSize);
            break;
            
        default: // unreachable
            Error=unrar_err_corrupt;
            break;
    }
    
    if (Error!=unrar_ok)
        return Error;
    
    if (ReadSize>0 && NextBlockPos<=CurBlockPos)
        return unrar_err_corrupt;

    *ReadSize_ = ReadSize;
    
	return unrar_ok;
}


unrar_err_t Archive::ReadHeader15(size_t *ReadSize)
{
    Raw.Reset();
    
    Raw.Read(SIZEOF_SHORTBLOCKHEAD);
    if (Raw.Size()==0)
        return unrar_err_corrupt;
    
    ShortBlock.HeadCRC=Raw.Get2();
    
    ShortBlock.Reset();
    
    uint HeaderType=Raw.Get1();
    ShortBlock.Flags=Raw.Get2();
    ShortBlock.SkipIfUnknown=(ShortBlock.Flags & SKIP_IF_UNKNOWN)!=0;
    ShortBlock.HeadSize=Raw.Get2();
    
    ShortBlock.HeaderType=(HEADER_TYPE)HeaderType;
    if (ShortBlock.HeadSize<SIZEOF_SHORTBLOCKHEAD)
        return unrar_err_corrupt;
    
    // For simpler further processing we map header types common
    // for RAR 1.5 and 5.0 formats to RAR 5.0 values. It does not include
    // header types specific for RAR 1.5 - 4.x only.
    switch(ShortBlock.HeaderType)
    {
        case HEAD3_MAIN:    ShortBlock.HeaderType=HEAD_MAIN;     break;
        case HEAD3_FILE:    ShortBlock.HeaderType=HEAD_FILE;     break;
        case HEAD3_SERVICE: ShortBlock.HeaderType=HEAD_SERVICE;  break;
        case HEAD3_ENDARC:  ShortBlock.HeaderType=HEAD_ENDARC;   break;
        default: break;
    }
    CurHeaderType=ShortBlock.HeaderType;
    
    if (ShortBlock.HeaderType==HEAD3_CMT)
    {
        // Old style (up to RAR 2.9) comment header embedded into main
        // or file header. We must not read the entire ShortBlock.HeadSize here
        // to not break the comment processing logic later.
        Raw.Read(SIZEOF_COMMHEAD-SIZEOF_SHORTBLOCKHEAD);
    }
    else
        if (ShortBlock.HeaderType==HEAD_MAIN && (ShortBlock.Flags & MHD_COMMENT)!=0)
        {
            // Old style (up to RAR 2.9) main archive comment embedded into
            // the main archive header found. While we can read the entire
            // ShortBlock.HeadSize here and remove this part of "if", it would be
            // waste of memory, because we'll read and process this comment data
            // in other function anyway and we do not need them here now.
            Raw.Read(SIZEOF_MAINHEAD3-SIZEOF_SHORTBLOCKHEAD);
        }
        else
            Raw.Read(ShortBlock.HeadSize-SIZEOF_SHORTBLOCKHEAD);
    
    NextBlockPos=CurBlockPos+ShortBlock.HeadSize;
    
    switch(ShortBlock.HeaderType)
    {
        case HEAD_MAIN:
            MainHead.Reset();
            *(BaseBlock *)&MainHead=ShortBlock;
            MainHead.HighPosAV=Raw.Get2();
            MainHead.PosAV=Raw.Get4();
            
            Solid=(MainHead.Flags & MHD_SOLID)!=0;
            MainHead.CommentInHeader=(MainHead.Flags & MHD_COMMENT)!=0;
            break;
        case HEAD_FILE:
        case HEAD_SERVICE:
        {
            bool FileBlock=ShortBlock.HeaderType==HEAD_FILE;
            FileHeader *hd=FileBlock ? &FileHead:&SubHead;
            hd->Reset();
            
            *(BaseBlock *)hd=ShortBlock;
            
            hd->SplitBefore=(hd->Flags & LHD_SPLIT_BEFORE)!=0;
            hd->SplitAfter=(hd->Flags & LHD_SPLIT_AFTER)!=0;
            hd->Encrypted=(hd->Flags & LHD_PASSWORD)!=0;
            hd->Solid=FileBlock && (hd->Flags & LHD_SOLID)!=0;
            hd->SubBlock=!FileBlock && (hd->Flags & LHD_SOLID)!=0;
            hd->Dir=(hd->Flags & LHD_WINDOWMASK)==LHD_DIRECTORY;
            hd->WinSize=hd->Dir ? 0:0x10000<<((hd->Flags & LHD_WINDOWMASK)>>5);
            hd->CommentInHeader=(hd->Flags & LHD_COMMENT)!=0;
            hd->Version=(hd->Flags & LHD_VERSION)!=0;
            
            hd->DataSize=Raw.Get4();
            uint LowUnpSize=Raw.Get4();
            hd->HostOS=Raw.Get1();
            
            hd->FileHash.Type=HASH_CRC32;
            hd->FileHash.CRC32=Raw.Get4();
            
            uint FileTime=Raw.Get4();
            hd->UnpVer=Raw.Get1();
            hd->Method=Raw.Get1()-0x30;
            size_t NameSize=Raw.Get2();
            hd->FileAttr=Raw.Get4();
            
            if (hd->Encrypted)
                return unrar_err_encrypted;
            
            hd->HSType=HSYS_UNKNOWN;
            if (hd->HostOS==HOST_UNIX || hd->HostOS==HOST_BEOS)
                hd->HSType=HSYS_UNIX;
            else
                if (hd->HostOS<HOST_MAX)
                    hd->HSType=HSYS_WINDOWS;
            
            hd->RedirType=FSREDIR_NONE;
            
            // RAR 4.x Unix symlink.
            if (hd->HostOS==HOST_UNIX && (hd->FileAttr & 0xF000)==0xA000)
            {
                hd->RedirType=FSREDIR_UNIXSYMLINK;
                *hd->RedirName=0;
            }
            
            hd->Inherited=!FileBlock && (hd->SubFlags & SUBHEAD_FLAGS_INHERITED)!=0;
            
            hd->LargeFile=(hd->Flags & LHD_LARGE)!=0;
            
            uint HighPackSize,HighUnpSize;
            if (hd->LargeFile)
            {
                HighPackSize=Raw.Get4();
                HighUnpSize=Raw.Get4();
                hd->UnknownUnpSize=(LowUnpSize==0xffffffff && HighUnpSize==0xffffffff);
            }
            else
            {
                HighPackSize=HighUnpSize=0;
                // UnpSize equal to 0xffffffff without LHD_LARGE flag indicates
                // that we do not know the unpacked file size and must unpack it
                // until we find the end of file marker in compressed data.
                hd->UnknownUnpSize=(LowUnpSize==0xffffffff);
            }
            hd->PackSize=int32to64(HighPackSize,hd->DataSize);
            hd->UnpSize=int32to64(HighUnpSize,LowUnpSize);
            if (hd->UnknownUnpSize)
                hd->UnpSize=INT64NDF;
            
            char FileName[NM*4];
            size_t ReadNameSize=Min(NameSize,ASIZE(FileName)-1);
            Raw.GetB((byte *)FileName,ReadNameSize);
            FileName[ReadNameSize]=0;
            
            if (FileBlock)
            {
                if ((hd->Flags & LHD_UNICODE)!=0)
                {
                    EncodeFileName NameCoder;
                    size_t Length=strlen(FileName);
                    Length++;
                    NameCoder.Decode(FileName,(byte *)FileName+Length,
                                     NameSize-Length,hd->FileName,
                                     ASIZE(hd->FileName));
                }
                else
                    *hd->FileName=0;
                
                char AnsiName[NM];
                IntToExt(FileName,AnsiName,ASIZE(AnsiName));
                GetWideName(AnsiName,hd->FileName,hd->FileName,ASIZE(hd->FileName));
                
                ConvertFileHeader(hd);
            }
            else
            {
                CharToWide(FileName,hd->FileName,ASIZE(hd->FileName));
                
                // Calculate the size of optional data.
                int DataSize=int(hd->HeadSize-NameSize-SIZEOF_FILEHEAD3);
                if ((hd->Flags & LHD_SALT)!=0)
                    return unrar_err_encrypted;
                
                if (DataSize>0)
                {
                    // Here we read optional additional fields for subheaders.
                    // They are stored after the file name and before salt.
                    hd->SubData.Alloc(DataSize);
                    Raw.GetB(&hd->SubData[0],DataSize);
                }
            }
            if ((hd->Flags & LHD_SALT)!=0)
                return unrar_err_encrypted;
            hd->mtime.SetDos(FileTime);
            if ((hd->Flags & LHD_EXTTIME)!=0)
            {
                ushort Flags=Raw.Get2();
                RarTime *tbl[4];
                tbl[0]=&FileHead.mtime;
                tbl[1]=&FileHead.ctime;
                tbl[2]=&FileHead.atime;
                tbl[3]=NULL; // Archive time is not used now.
                for (int I=0;I<4;I++)
                {
                    RarTime *CurTime=tbl[I];
                    uint rmode=Flags>>(3-I)*4;
                    if ((rmode & 8)==0 || CurTime==NULL)
                        continue;
                    if (I!=0)
                    {
                        uint DosTime=Raw.Get4();
                        CurTime->SetDos(DosTime);
                    }
                    RarLocalTime rlt;
                    CurTime->GetLocal(&rlt);
                    if (rmode & 4)
                        rlt.Second++;
                    rlt.Reminder=0;
                    int count=rmode&3;
                    for (int J=0;J<count;J++)
                    {
                        byte CurByte=Raw.Get1();
                        rlt.Reminder|=(((uint)CurByte)<<((J+3-count)*8));
                    }
                    CurTime->SetLocal(&rlt);
                }
            }
            NextBlockPos+=hd->PackSize;
            bool CRCProcessedOnly=hd->CommentInHeader;
            ushort HeaderCRC=Raw.GetCRC15(CRCProcessedOnly);
            if (hd->HeadCRC!=HeaderCRC)
                return unrar_err_corrupt;
        }
            break;
        case HEAD_ENDARC:
            *(BaseBlock *)&EndArcHead=ShortBlock;
            EndArcHead.NextVolume=(EndArcHead.Flags & EARC_NEXT_VOLUME)!=0;
            EndArcHead.DataCRC=(EndArcHead.Flags & EARC_DATACRC)!=0;
            EndArcHead.RevSpace=(EndArcHead.Flags & EARC_REVSPACE)!=0;
            EndArcHead.StoreVolNumber=(EndArcHead.Flags & EARC_VOLNUMBER)!=0;
            if (EndArcHead.DataCRC)
                EndArcHead.ArcDataCRC=Raw.Get4();
            if (EndArcHead.StoreVolNumber)
                return unrar_err_segmented;
            break;
        default:
            if (ShortBlock.Flags & LONG_BLOCK)
                NextBlockPos+=Raw.Get4();
            break;
    }
    
    ushort HeaderCRC=Raw.GetCRC15(false);
    
    // Old AV header does not have header CRC properly set.
    if (ShortBlock.HeadCRC!=HeaderCRC && ShortBlock.HeaderType!=HEAD3_SIGN &&
        ShortBlock.HeaderType!=HEAD3_AV)
        return unrar_err_corrupt;
    
    if (NextBlockPos<=CurBlockPos)
        return unrar_err_corrupt;
    
    *ReadSize=Raw.Size();
    return unrar_ok;
}


unrar_err_t Archive::ReadHeader50(size_t *ReadSize)
{
    Raw.Reset();
    
    // Header size must not occupy more than 3 variable length integer bytes
    // resulting in 2 MB maximum header size, so here we read 4 byte CRC32
    // followed by 3 bytes or less of header size.
    const size_t FirstReadSize=7; // Smallest possible block size.
    if (Raw.Read(FirstReadSize)<FirstReadSize)
        return unrar_err_arc_eof;
    
    ShortBlock.Reset();
    ShortBlock.HeadCRC=Raw.Get4();
    uint SizeBytes=Raw.GetVSize(4);
    uint64 BlockSize=Raw.GetV();
    
    if (BlockSize==0 || SizeBytes==0)
        return unrar_err_corrupt;
    
    int SizeToRead=int(BlockSize);
    SizeToRead-=FirstReadSize-SizeBytes-4; // Adjust overread size bytes if any.
    uint HeaderSize=4+SizeBytes+(uint)BlockSize;
    
    if (SizeToRead<0 || HeaderSize<SIZEOF_SHORTBLOCKHEAD5)
        return unrar_err_corrupt;
    
    Raw.Read(SizeToRead);
    
    if (Raw.Size()<HeaderSize)
        return unrar_err_arc_eof;
    
    uint HeaderCRC=Raw.GetCRC50();
    
    ShortBlock.HeaderType=(HEADER_TYPE)Raw.GetV();
    ShortBlock.Flags=(uint)Raw.GetV();
    ShortBlock.SkipIfUnknown=(ShortBlock.Flags & HFL_SKIPIFUNKNOWN)!=0;
    ShortBlock.HeadSize=HeaderSize;
    
    CurHeaderType=ShortBlock.HeaderType;
    
    bool BadCRC=(ShortBlock.HeadCRC!=HeaderCRC);
    if (BadCRC)
        return unrar_err_corrupt;
    
    uint64 ExtraSize=0;
    if ((ShortBlock.Flags & HFL_EXTRA)!=0)
    {
        ExtraSize=Raw.GetV();
        if (ExtraSize>=ShortBlock.HeadSize)
            return unrar_err_corrupt;
    }
    
    uint64 DataSize=0;
    if ((ShortBlock.Flags & HFL_DATA)!=0)
        DataSize=Raw.GetV();
    
    NextBlockPos=CurBlockPos+ShortBlock.HeadSize+DataSize;
    
    switch(ShortBlock.HeaderType)
    {
        case HEAD_CRYPT:
            return unrar_err_encrypted;
        case HEAD_MAIN:
        {
            MainHead.Reset();
            *(BaseBlock *)&MainHead=ShortBlock;
            uint ArcFlags=(uint)Raw.GetV();
            
            Solid=(ArcFlags & MHFL_SOLID)!=0;
            
            if (ExtraSize!=0)
            {
                unrar_err_t Error;
                if ((Error=ProcessExtra50(&Raw,(size_t)ExtraSize,&MainHead))!=unrar_ok)
                    return Error;
            }
        }
            break;
        case HEAD_FILE:
        case HEAD_SERVICE:
        {
            FileHeader *hd=ShortBlock.HeaderType==HEAD_FILE ? &FileHead:&SubHead;
            hd->Reset();
            *(BaseBlock *)hd=ShortBlock;
            
            bool FileBlock=ShortBlock.HeaderType==HEAD_FILE;
            
            hd->LargeFile=true;
            
            hd->PackSize=DataSize;
            hd->FileFlags=(uint)Raw.GetV();
            hd->UnpSize=Raw.GetV();
            
            hd->UnknownUnpSize=(hd->FileFlags & FHFL_UNPUNKNOWN)!=0;
            if (hd->UnknownUnpSize)
                hd->UnpSize=INT64NDF;
            
            hd->MaxSize=Max(hd->PackSize,hd->UnpSize);
            hd->FileAttr=(uint)Raw.GetV();
            if ((hd->FileFlags & FHFL_UTIME)!=0)
                hd->mtime=(time_t)Raw.Get4();
            
            hd->FileHash.Type=HASH_NONE;
            if ((hd->FileFlags & FHFL_CRC32)!=0)
            {
                hd->FileHash.Type=HASH_CRC32;
                hd->FileHash.CRC32=Raw.Get4();
            }
            
            hd->RedirType=FSREDIR_NONE;
            
            uint CompInfo=(uint)Raw.GetV();
            hd->Method=(CompInfo>>7) & 7;
            hd->UnpVer=CompInfo & 0x3f;
            
            hd->HostOS=(byte)Raw.GetV();
            size_t NameSize=(size_t)Raw.GetV();
            hd->Inherited=(ShortBlock.Flags & HFL_INHERITED)!=0;
            
            hd->HSType=HSYS_UNKNOWN;
            if (hd->HostOS==HOST5_UNIX)
                hd->HSType=HSYS_UNIX;
            else
                if (hd->HostOS==HOST5_WINDOWS)
                    hd->HSType=HSYS_WINDOWS;
            
            hd->SplitBefore=(hd->Flags & HFL_SPLITBEFORE)!=0;
            hd->SplitAfter=(hd->Flags & HFL_SPLITAFTER)!=0;
            hd->SubBlock=(hd->Flags & HFL_CHILD)!=0;
            hd->Solid=FileBlock && (CompInfo & FCI_SOLID)!=0;
            hd->Dir=(hd->FileFlags & FHFL_DIRECTORY)!=0;
            hd->WinSize=hd->Dir ? 0:size_t(0x20000)<<((CompInfo>>10)&0xf);
            
            if (hd->Encrypted)
                return unrar_err_encrypted;
            
            char FileName[NM*4];
            size_t ReadNameSize=Min(NameSize,ASIZE(FileName)-1);
            Raw.GetB((byte *)FileName,ReadNameSize);
            FileName[ReadNameSize]=0;
            
            UtfToWide(FileName,hd->FileName,ASIZE(hd->FileName)-1);
            
            // Should do it before converting names, because extra fields can
            // affect name processing, like in case of NTFS streams.
            if (ExtraSize!=0)
                ProcessExtra50(&Raw,(size_t)ExtraSize,hd);
            
            if (FileBlock)
                ConvertFileHeader(hd);
            
            if (BadCRC) // Add the file name to broken header message displayed above.
                return unrar_err_corrupt;
        }
            break;
        case HEAD_ENDARC:
        {
            *(BaseBlock *)&EndArcHead=ShortBlock;
            uint ArcFlags=(uint)Raw.GetV();
            EndArcHead.NextVolume=(ArcFlags & EHFL_NEXTVOLUME)!=0;
            EndArcHead.StoreVolNumber=false;
            EndArcHead.DataCRC=false;
            EndArcHead.RevSpace=false;
        }
            break;
    }
    
    if (NextBlockPos<=CurBlockPos)
        return unrar_err_corrupt;
    
    *ReadSize=Raw.Size();
    
    return unrar_ok;
}


unrar_err_t Archive::ProcessExtra50(RawRead *Raw,size_t ExtraSize,BaseBlock *bb)
{
    // Read extra data from the end of block skipping any fields before it.
    size_t ExtraStart=Raw->Size()-ExtraSize;
    if (ExtraStart<Raw->GetPos())
        return unrar_err_corrupt;
    Raw->SetPos(ExtraStart);
    while (Raw->DataLeft()>=2)
    {
        int64 FieldSize=Raw->GetV();
        if (FieldSize==0 || Raw->DataLeft()==0 || FieldSize>(int64)Raw->DataLeft())
            break;
        size_t NextPos=size_t(Raw->GetPos()+FieldSize);
        uint64 FieldType=Raw->GetV();
        
        FieldSize=Raw->DataLeft(); // Field size without size and type fields.
        
        if (bb->HeaderType==HEAD_MAIN)
        {
            MainHeader *hd=(MainHeader *)bb;
            if (FieldType==MHEXTRA_LOCATOR)
            {
                hd->Locator=true;
                uint Flags=(uint)Raw->GetV();
                if ((Flags & MHEXTRA_LOCATOR_QLIST)!=0)
                {
                    uint64 Offset=Raw->GetV();
                    if (Offset!=0) // 0 means that reserved space was not enough to write the offset.
                        hd->QOpenOffset=Offset+CurBlockPos;
                }
                if ((Flags & MHEXTRA_LOCATOR_RR)!=0)
                {
                    uint64 Offset=Raw->GetV();
                    if (Offset!=0) // 0 means that reserved space was not enough to write the offset.
                        hd->RROffset=Offset+CurBlockPos;
                }
            }
        }
        
        if (bb->HeaderType==HEAD_FILE || bb->HeaderType==HEAD_SERVICE)
        {
            FileHeader *hd=(FileHeader *)bb;
            switch(FieldType)
            {
                case FHEXTRA_CRYPT:
                    return unrar_err_encrypted;
                case FHEXTRA_HASH:
                {
                    FileHeader *hd=(FileHeader *)bb;
                    uint Type=(uint)Raw->GetV();
                    if (Type==FHEXTRA_HASH_BLAKE2)
                    {
                        hd->FileHash.Type=HASH_BLAKE2;
                        Raw->GetB(hd->FileHash.Digest,BLAKE2_DIGEST_SIZE);
                    }
                }
                    break;
                case FHEXTRA_HTIME:
                    if (FieldSize>=9)
                    {
                        byte Flags=(byte)Raw->GetV();
                        bool UnixTime=(Flags & FHEXTRA_HTIME_UNIXTIME)!=0;
                        if ((Flags & FHEXTRA_HTIME_MTIME)!=0)
                        {
                            if (UnixTime)
                                hd->mtime=(time_t)Raw->Get4();
                            else
                                hd->mtime.SetRaw(Raw->Get8());
                        }
                        if ((Flags & FHEXTRA_HTIME_CTIME)!=0)
                        {
                            if (UnixTime)
                                hd->ctime=(time_t)Raw->Get4();
                            else
                                hd->ctime.SetRaw(Raw->Get8());
                        }
                        if ((Flags & FHEXTRA_HTIME_ATIME)!=0)
                        {
                            if (UnixTime)
                                hd->atime=(time_t)Raw->Get4();
                            else
                                hd->atime.SetRaw(Raw->Get8());
                        }
                    }
                    break;
                case FHEXTRA_REDIR:
                {
                    hd->RedirType=(FILE_SYSTEM_REDIRECT)Raw->GetV();
                    uint Flags=(uint)Raw->GetV();
                    hd->DirTarget=(Flags & FHEXTRA_REDIR_DIR)!=0;
                    size_t NameSize=(size_t)Raw->GetV();
                    
                    char UtfName[NM*4];
                    *UtfName=0;
                    if (NameSize<ASIZE(UtfName)-1)
                    {
                        Raw->GetB(UtfName,NameSize);
                        UtfName[NameSize]=0;
                    }
#ifdef _WIN_ALL
                    UnixSlashToDos(UtfName,UtfName,ASIZE(UtfName));
#endif
                    UtfToWide(UtfName,hd->RedirName,ASIZE(hd->RedirName));
                }
                    break;
                case FHEXTRA_UOWNER:
                {
                    uint Flags=(uint)Raw->GetV();
                    hd->UnixOwnerNumeric=(Flags & FHEXTRA_UOWNER_NUMUID)!=0;
                    hd->UnixGroupNumeric=(Flags & FHEXTRA_UOWNER_NUMGID)!=0;
                    *hd->UnixOwnerName=*hd->UnixGroupName=0;
                    if ((Flags & FHEXTRA_UOWNER_UNAME)!=0)
                    {
                        size_t Length=(size_t)Raw->GetV();
                        Length=Min(Length,ASIZE(hd->UnixOwnerName)-1);
                        Raw->GetB(hd->UnixOwnerName,Length);
                        hd->UnixOwnerName[Length]=0;
                    }
                    if ((Flags & FHEXTRA_UOWNER_GNAME)!=0)
                    {
                        size_t Length=(size_t)Raw->GetV();
                        Length=Min(Length,ASIZE(hd->UnixGroupName)-1);
                        Raw->GetB(hd->UnixGroupName,Length);
                        hd->UnixGroupName[Length]=0;
                    }
#ifdef _UNIX
                    if (hd->UnixOwnerNumeric)
                        hd->UnixOwnerID=(uid_t)Raw->GetV();
                    if (hd->UnixGroupNumeric)
                        hd->UnixGroupID=(uid_t)Raw->GetV();
#else
                    // Need these fields in Windows too for 'list' command,
                    // but uid_t and gid_t are not defined.
                    if (hd->UnixOwnerNumeric)
                        hd->UnixOwnerID=(uint)Raw->GetV();
                    if (hd->UnixGroupNumeric)
                        hd->UnixGroupID=(uint)Raw->GetV();
#endif
                    hd->UnixOwnerSet=true;
                }
                    break;
                case FHEXTRA_SUBDATA:
                {
                    hd->SubData.Alloc((size_t)FieldSize);
                    Raw->GetB(hd->SubData.Addr(0),(size_t)FieldSize);
                }
                    break;
            }
        }
        
        Raw->SetPos(NextPos);
    }
    
    return unrar_ok;
}


#ifndef SFX_MODULE
unrar_err_t Archive::ReadHeader14(size_t *ReadSize)
{
    Raw.Reset();
    
    if (CurBlockPos<=(int64)SFXSize)
    {
        Raw.Read(SIZEOF_MAINHEAD14);
        MainHead.Reset();
        byte Mark[4];
        Raw.GetB(Mark,4);
        uint HeadSize=Raw.Get2();
        byte Flags=Raw.Get1();
        NextBlockPos=CurBlockPos+HeadSize;
        CurHeaderType=HEAD_MAIN;
        
        Solid=(Flags & MHD_SOLID)!=0;
        MainHead.CommentInHeader=(Flags & MHD_COMMENT)!=0;
        MainHead.PackComment=(Flags & MHD_PACK_COMMENT)!=0;
    }
    else
    {
        Raw.Read(SIZEOF_FILEHEAD14);
        FileHead.Reset();
        
        FileHead.HeaderType=HEAD_FILE;
        FileHead.DataSize=Raw.Get4();
        FileHead.UnpSize=Raw.Get4();
        FileHead.FileHash.Type=HASH_RAR14;
        FileHead.FileHash.CRC32=Raw.Get2();
        FileHead.HeadSize=Raw.Get2();
        uint FileTime=Raw.Get4();
        FileHead.FileAttr=Raw.Get1();
        FileHead.Flags=Raw.Get1()|LONG_BLOCK;
        FileHead.UnpVer=(Raw.Get1()==2) ? 13 : 10;
        size_t NameSize=Raw.Get1();
        FileHead.Method=Raw.Get1();
        
        FileHead.SplitBefore=(FileHead.Flags & LHD_SPLIT_BEFORE)!=0;
        FileHead.SplitAfter=(FileHead.Flags & LHD_SPLIT_AFTER)!=0;
        FileHead.Encrypted=(FileHead.Flags & LHD_PASSWORD)!=0;
        if (FileHead.Encrypted)
            return unrar_err_encrypted;
            
        FileHead.PackSize=FileHead.DataSize;
        FileHead.WinSize=0x10000;
        
        FileHead.mtime.SetDos(FileTime);
        
        Raw.Read(NameSize);
        
        char FileName[NM];
        Raw.GetB((byte *)FileName,Min(NameSize,ASIZE(FileName)));
        FileName[NameSize]=0;
        IntToExt(FileName,FileName,ASIZE(FileName));
        CharToWide(FileName,FileHead.FileName,ASIZE(FileHead.FileName));
        
        if (Raw.Size()!=0)
            NextBlockPos=CurBlockPos+FileHead.HeadSize+FileHead.PackSize;
        CurHeaderType=HEAD_FILE;
    }
    *ReadSize=(NextBlockPos>CurBlockPos ? Raw.Size():0);
    return unrar_ok;
}
#endif


// (removed name case and attribute conversion)

bool Archive::IsArcDir()
{
    return FileHead.Dir;
}


bool Archive::IsArcLabel()
{
    return(FileHead.HostOS<=HOST_WIN32 && (FileHead.FileAttr & 8));
}


void Archive::ConvertFileHeader(FileHeader *hd)
{
    if (Format==RARFMT15 && hd->UnpVer<20 && (hd->FileAttr & 0x10))
        hd->Dir=true;
    if (hd->HSType==HSYS_UNKNOWN)
        if (hd->Dir)
            hd->FileAttr=0x10;
        else
            hd->FileAttr=0x20;
}


int64 Archive::GetStartPos()
{
    int64 StartPos=SFXSize+MarkHead.HeadSize;
    StartPos+=MainHead.HeadSize;
    return StartPos;
}
