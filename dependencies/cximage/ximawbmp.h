/*
 * File:	ximawbmp.h
 * Purpose:	WBMP Image Class Loader and Writer
 */
/* ==========================================================
 * CxImageWBMP (c) 12/Jul/2002 Davide Pizzolato - www.xdp.it
 * For conditions of distribution and use, see copyright notice in ximage.h
 * ==========================================================
 */
#if !defined(__ximaWBMP_h)
#define __ximaWBMP_h

#include "ximage.h"

#if CXIMAGE_SUPPORT_WBMP

class CxImageWBMP: public CxImage
{
#pragma pack(1)
typedef struct tagWbmpHeader
{
    BYTE   Type;            // 0
    BYTE   FixHeader;       // 0
    BYTE   ImageWidth;      // Image Width
    BYTE   ImageHeight;     // Image Height
} WBMPHEADER;
#pragma pack()
public:
	CxImageWBMP(): CxImage(CXIMAGE_FORMAT_WBMP) {}

//	bool Load(const char * imageFileName){ return CxImage::Load(imageFileName,CXIMAGE_FORMAT_WBMP);}
//	bool Save(const char * imageFileName){ return CxImage::Save(imageFileName,CXIMAGE_FORMAT_WBMP);}
	bool Decode(CxFile * hFile);
	bool Decode(FILE *hFile) { CxIOFile file(hFile); return Decode(&file); }

#if CXIMAGE_SUPPORT_ENCODE
	bool Encode(CxFile * hFile);
	bool Encode(FILE *hFile) { CxIOFile file(hFile); return Encode(&file); }
#endif // CXIMAGE_SUPPORT_ENCODE
};

#endif

#endif
