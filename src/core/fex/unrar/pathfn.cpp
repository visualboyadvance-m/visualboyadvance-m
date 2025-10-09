#include "rar.hpp"

wchar* GetWideName(const char *Name,const wchar *NameW,wchar *DestW,size_t DestSize)
{
    if (NameW!=NULL && *NameW!=0)
    {
        if (DestW!=NameW)
            my_wcsncpy(DestW,NameW,DestSize);
    }
    else
        if (Name!=NULL)
            CharToWide(Name,DestW,DestSize);
        else
            *DestW=0;
    
    // Ensure that we return a zero terminate string for security reasons.
    if (DestSize>0)
        DestW[DestSize-1]=0;
    
    return(DestW);
}

void UnixSlashToDos(const char *SrcName, char *DestName, size_t MaxLength)
{
	size_t Copied = 0;
	for (; Copied<MaxLength - 1 && SrcName[Copied] != 0; Copied++)
		DestName[Copied] = SrcName[Copied] == '/' ? '\\' : SrcName[Copied];
	DestName[Copied] = 0;
}


void DosSlashToUnix(const char *SrcName, char *DestName, size_t MaxLength)
{
	size_t Copied = 0;
	for (; Copied<MaxLength - 1 && SrcName[Copied] != 0; Copied++)
		DestName[Copied] = SrcName[Copied] == '\\' ? '/' : SrcName[Copied];
	DestName[Copied] = 0;
}


void UnixSlashToDos(const wchar *SrcName, wchar *DestName, size_t MaxLength)
{
	size_t Copied = 0;
	for (; Copied<MaxLength - 1 && SrcName[Copied] != 0; Copied++)
		DestName[Copied] = SrcName[Copied] == '/' ? '\\' : SrcName[Copied];
	DestName[Copied] = 0;
}


void DosSlashToUnix(const wchar *SrcName, wchar *DestName, size_t MaxLength)
{
	size_t Copied = 0;
	for (; Copied<MaxLength - 1 && SrcName[Copied] != 0; Copied++)
		DestName[Copied] = SrcName[Copied] == '\\' ? '/' : SrcName[Copied];
	DestName[Copied] = 0;
}
