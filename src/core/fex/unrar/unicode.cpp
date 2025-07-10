#include "rar.hpp"

#include "unicode.hpp"

bool CharToWide(const char *Src,wchar *Dest,int DestSize)
{
    bool RetCode=true;
#ifdef _WIN_32
    if (MultiByteToWideChar(CP_ACP,0,Src,-1,Dest,DestSize,NULL,NULL)==0)
        RetCode=false;
#else
#ifdef _APPLE
    UtfToWide(Src,Dest,DestSize);
#else
#ifdef MBFUNCTIONS
    
    size_t ResultingSize=mbstowcs(Dest,Src,DestSize);
    if (ResultingSize==(size_t)-1)
        RetCode=false;
    if (ResultingSize==0 && *Src!=0)
        RetCode=false;
    
    if ((!RetCode || *Dest==0 && *Src!=0) && DestSize>NM && strlen(Src)<NM)
    {
        /* Workaround for strange Linux Unicode functions bug.
         Some of wcstombs and mbstowcs implementations in some situations
         (we are yet to find out what it depends on) can return an empty
         string and success code if buffer size value is too large.
         */
        return(CharToWide(Src,Dest,NM));
    }
    
#else
    // TODO: convert to UTF-8? Would need conversion routine. Ugh.
    for (int I=0;I<DestSize;I++)
    {
        Dest[I]=(wchar)Src[I];
        if (Src[I]==0)
            break;
    }
#endif
#endif
#endif
    return(RetCode);
}

bool WideToChar(const wchar *Src,char *Dest,int DestSize)
{
	bool RetCode=true;
#ifdef _WIN_32
	if (WideCharToMultiByte(CP_ACP,0,Src,-1,Dest,DestSize,NULL,NULL)==0)
		RetCode=false;
#else
#ifdef _APPLE
	WideToUtf(Src,Dest,DestSize);
#else
#ifdef MBFUNCTIONS

	size_t ResultingSize=wcstombs(Dest,Src,DestSize);
	if (ResultingSize==(size_t)-1)
		RetCode=false;
	if (ResultingSize==0 && *Src!=0)
		RetCode=false;

	if ((!RetCode || *Dest==0 && *Src!=0) && DestSize>NM && strlenw(Src)<NM)
	{
		/* Workaround for strange Linux Unicode functions bug.
			 Some of wcstombs and mbstowcs implementations in some situations
			 (we are yet to find out what it depends on) can return an empty
			 string and success code if buffer size value is too large.
		*/
		return(WideToChar(Src,Dest,NM));
	}

#else
	// TODO: convert to UTF-8? Would need conversion routine. Ugh.
	for (int I=0;I<DestSize;I++)
	{
		Dest[I]=(char)Src[I];
		if (Src[I]==0)
			break;
	}
#endif
#endif
#endif
	return(RetCode);
}

void UtfToWide(const char *Src,wchar *Dest,int DestSize)
{
	DestSize--;
	while (*Src!=0)
	{
		uint c=(byte)*(Src++),d;
		if (c<0x80)
			d=c;
		else
			if ((c>>5)==6)
			{
				if ((*Src&0xc0)!=0x80)
					break;
				d=((c&0x1f)<<6)|(*Src&0x3f);
				Src++;
			}
			else
				if ((c>>4)==14)
				{
					if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80)
						break;
					d=((c&0xf)<<12)|((Src[0]&0x3f)<<6)|(Src[1]&0x3f);
					Src+=2;
				}
				else
					if ((c>>3)==30)
					{
						if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80 || (Src[2]&0xc0)!=0x80)
							break;
						d=((c&7)<<18)|((Src[0]&0x3f)<<12)|((Src[1]&0x3f)<<6)|(Src[2]&0x3f);
						Src+=3;
					}
					else
						break;
		if (--DestSize<0)
			break;
		if (d>0xffff)
		{
			if (--DestSize<0 || d>0x10ffff)
				break;
			*(Dest++)=((d-0x10000)>>10)+0xd800;
			*(Dest++)=(d&0x3ff)+0xdc00;
		}
		else
			*(Dest++)=d;
	}
	*Dest=0;
}


wchar* RawToWide(const byte *Src,wchar *Dest,size_t DestSize)
{
    for (size_t I=0;I<DestSize;I++)
        if ((Dest[I]=Src[I*2]+(Src[I*2+1]<<8))==0)
            break;
    return Dest;
}


// strfn.cpp
void ExtToInt(const char *Src,char *Dest)
{
#if defined(_WIN_32)
	CharToOem(Src,Dest);
#else
	if (Dest!=Src)
		strcpy(Dest,Src);
#endif
}

size_t my_wcslen(const wchar *a)
{
    size_t I;
    for (I=0; a[I]; ++I);
    return I;
}

int my_wcsncmp(const wchar *a, const wchar *b, size_t Max)
{
    for (size_t I=0; I<Max && (a[I] || b[I]); ++I)
    {
        if (a[I] != b[I])
            return a[I] - b[I];
    }
    return 0;
}

int my_wcscmp(const wchar *a, const wchar *b)
{
    return my_wcsncmp(a, b, ~0UL);
}

void my_wcsncpy(wchar *Dest, const wchar *Src, size_t DestSize)
{
    size_t I;
    for (I=0; I<DestSize && Src[I]; ++I)
        Dest[I] = Src[I];
    if (I<DestSize)
        Dest[I] = 0;
}

void my_wcsncat(wchar *Dest, const wchar *Src, size_t DestSize)
{
    size_t I, J;
    for (I=0; I<DestSize && Dest[I]; ++I);
    for (J=0; I<DestSize && Src[J]; ++I, ++J)
        Dest[I] = Src[J];
    if (I<DestSize)
        Dest[I] = 0;
}
