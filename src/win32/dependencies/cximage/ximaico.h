/*
 * File:	ximaico.h
 * Purpose:	ICON Image Class Loader and Writer
 */
/* ==========================================================
 * CxImageICO (c) 07/Aug/2001 Davide Pizzolato - www.xdp.it
 * For conditions of distribution and use, see copyright notice in ximage.h
 * ==========================================================
 */
#if !defined(__ximaICO_h)
#define __ximaICO_h

#include "ximage.h"

#if CXIMAGE_SUPPORT_ICO

class CxImageICO: public CxImage
{
typedef struct tagIconDirectoryEntry {
    BYTE  bWidth;
    BYTE  bHeight;
    BYTE  bColorCount;
    BYTE  bReserved;
    WORD  wPlanes;
    WORD  wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
} ICONDIRENTRY;

typedef struct tagIconDir {
    WORD          idReserved;
    WORD          idType;
    WORD          idCount;
} ICONHEADER;

public:
	CxImageICO(): CxImage(CXIMAGE_FORMAT_ICO) {}

//	bool Load(const char * imageFileName){ return CxImage::Load(imageFileName,CXIMAGE_FORMAT_ICO);}
//	bool Save(const char * imageFileName){ return CxImage::Save(imageFileName,CXIMAGE_FORMAT_ICO);}
	bool Decode(CxFile * hFile);
	bool Decode(FILE *hFile) { CxIOFile file(hFile); return Decode(&file); }

#if CXIMAGE_SUPPORT_ENCODE
	bool Encode(CxFile * hFile);
	bool Encode(FILE *hFile) { CxIOFile file(hFile); return Encode(&file); }
#endif // CXIMAGE_SUPPORT_ENCODE
};

#endif

#endif
