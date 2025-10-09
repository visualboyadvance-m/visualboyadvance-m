#include "rar.hpp"

RawRead::RawRead(ComprDataIO *SrcFile)
{
	RawRead::SrcFile=SrcFile;
    Reset();
}

void RawRead::Reset()
{
	ReadPos=0;
	DataSize=0;
	Data.Reset();
}

size_t RawRead::Read(size_t Size)
{
    size_t ReadSize=0;
	// (removed decryption)
    if (Size!=0)
    {
        Data.Add(Size);
        ReadSize=SrcFile->Read(&Data[DataSize],Size);
        DataSize+=ReadSize;
    }
    return ReadSize;
}


void RawRead::Read(byte *SrcData,size_t Size)
{
    if (Size!=0)
    {
        Data.Add(Size);
        memcpy(&Data[DataSize],SrcData,Size);
        DataSize+=Size;
    }
}


byte RawRead::Get1()
{
    return ReadPos<DataSize ? Data[ReadPos++]:0;
}


ushort RawRead::Get2()
{
    if (ReadPos+1<DataSize)
    {
        ushort Result=Data[ReadPos]+(Data[ReadPos+1]<<8);
        ReadPos+=2;
        return Result;
    }
    return 0;
}


uint RawRead::Get4()
{
    if (ReadPos+3<DataSize)
    {
        uint Result=Data[ReadPos]+(Data[ReadPos+1]<<8)+(Data[ReadPos+2]<<16)+
        (Data[ReadPos+3]<<24);
        ReadPos+=4;
        return Result;
    }
    return 0;
}


uint64 RawRead::Get8()
{
    uint Low=Get4(),High=Get4();
    return int32to64(High,Low);
}


uint64 RawRead::GetV()
{
    uint64 Result=0;
    for (uint Shift=0;ReadPos<DataSize;Shift+=7)
    {
        byte CurByte=Data[ReadPos++];
        Result+=uint64(CurByte & 0x7f)<<Shift;
        if ((CurByte & 0x80)==0)
            return Result; // Decoded successfully.
    }
    return 0; // Out of buffer border.
}


// Return a number of bytes in current variable length integer.
uint RawRead::GetVSize(size_t Pos)
{
    for (size_t CurPos=Pos;CurPos<DataSize;CurPos++)
        if ((Data[CurPos] & 0x80)==0)
            return int(CurPos-Pos+1);
    return 0; // Buffer overflow.
}


size_t RawRead::GetB(void *Field,size_t Size)
{
    byte *F=(byte *)Field;
    size_t CopySize=Min(DataSize-ReadPos,Size);
    if (CopySize>0)
        memcpy(F,&Data[ReadPos],CopySize);
    if (Size>CopySize)
        memset(F+CopySize,0,Size-CopySize);
    ReadPos+=CopySize;
    return CopySize;
}


void RawRead::GetW(wchar *Field,size_t Size)
{
    if (ReadPos+2*Size-1<DataSize)
    {
        RawToWide(&Data[ReadPos],Field,Size);
        ReadPos+=sizeof(wchar)*Size;
    }
    else
        memset(Field,0,sizeof(wchar)*Size);
}


uint RawRead::GetCRC15(bool ProcessedOnly) // RAR 1.5 block CRC.
{
    if (DataSize<=2)
        return 0;
    uint HeaderCRC=CRC32(0xffffffff,&Data[2],(ProcessedOnly ? ReadPos:DataSize)-2);
    return ~HeaderCRC & 0xffff;
}


uint RawRead::GetCRC50() // RAR 5.0 block CRC.
{
    if (DataSize<=4)
        return 0xffffffff;
    return CRC32(0xffffffff,&Data[4],DataSize-4) ^ 0xffffffff;
}


// Read vint from arbitrary byte array.
uint64 RawGetV(const byte *Data,uint &ReadPos,uint DataSize,bool &Overflow)
{
    Overflow=false;
    uint64 Result=0;
    for (uint Shift=0;ReadPos<DataSize;Shift+=7)
    {
        byte CurByte=Data[ReadPos++];
        Result+=uint64(CurByte & 0x7f)<<Shift;
        if ((CurByte & 0x80)==0)
            return Result; // Decoded successfully.
    }
    Overflow=true;
    return 0; // Out of buffer border.
}
