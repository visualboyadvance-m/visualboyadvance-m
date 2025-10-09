#ifndef _RAR_UNICODE_
#define _RAR_UNICODE_

bool CharToWide(const char *Src,wchar *Dest,int DestSize);
bool WideToChar(const wchar *Src,char *Dest,int DestSize=0x1000000);
void UtfToWide(const char *Src,wchar *Dest,int DestSize);
wchar* RawToWide(const byte *Src,wchar *Dest,size_t DestSize);

// strfn.cpp
void ExtToInt(const char *Src,char *Dest);

size_t my_wcslen(const wchar *a);
int my_wcscmp(const wchar *a, const wchar *b);
int my_wcsncmp(const wchar *a, const wchar *b, size_t DestSize);
void my_wcsncpy(wchar *Dest, const wchar *Src, size_t DestSize);
void my_wcsncat(wchar *Dest, const wchar *Src, size_t DestSize);

#endif
