/*
 * File:	ximaj2k.h
 * Purpose:	J2K Image Class Loader and Writer
 */
/* ==========================================================
 * CxImageJ2K (c) 04/Aug/2002 Davide Pizzolato - www.xdp.it
 * For conditions of distribution and use, see copyright notice in ximage.h
 *
 * based on LIBJ2K Copyright (c) 2001-2002, David Janssens - All rights reserved.
 * ==========================================================
 */
#if !defined(__ximaJ2K_h)
#define __ximaJ2K_h

#include "ximage.h"

#if CXIMAGE_SUPPORT_J2K

#define LIBJ2K_EXPORTS
extern "C" {
#include "../j2k/j2k.h"
};

class CxImageJ2K: public CxImage
{
public:
	CxImageJ2K(): CxImage(CXIMAGE_FORMAT_J2K) {}

//	bool Load(const char * imageFileName){ return CxImage::Load(imageFileName,CXIMAGE_FORMAT_J2K);}
//	bool Save(const char * imageFileName){ return CxImage::Save(imageFileName,CXIMAGE_FORMAT_J2K);}
	bool Decode(CxFile * hFile);
	bool Decode(FILE *hFile) { CxIOFile file(hFile); return Decode(&file); }

#if CXIMAGE_SUPPORT_ENCODE
	bool Encode(CxFile * hFile);
	bool Encode(FILE *hFile) { CxIOFile file(hFile); return Encode(&file); }
#endif // CXIMAGE_SUPPORT_ENCODE
protected:
	void j2k_calc_explicit_stepsizes(j2k_tccp_t *tccp, int prec);
	void j2k_encode_stepsize(int stepsize, int numbps, int *expn, int *mant);
	int j2k_floorlog2(int a);
};

#endif

#endif
