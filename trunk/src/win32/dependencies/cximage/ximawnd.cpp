// xImaWnd.cpp : Windows functions
/* 07/08/2001 v1.00 - Davide Pizzolato - www.xdp.it
 * CxImage version 5.99c 17/Oct/2004
 */

#include "ximage.h"

#include "ximaiter.h" 
#include "ximabmp.h"

////////////////////////////////////////////////////////////////////////////////
#if CXIMAGE_SUPPORT_WINCE
////////////////////////////////////////////////////////////////////////////////
long CxImage::Blt(HDC pDC, long x, long y)
{
	if((pDib==0)||(pDC==0)||(!info.bEnabled)) return 0;

    HBRUSH brImage = CreateDIBPatternBrushPt(pDib, DIB_RGB_COLORS);
    HBRUSH brOld = (HBRUSH) SelectObject(pDC, brImage);
    PatBlt(pDC, 0, 0, head.biWidth, head.biHeight, PATCOPY);
    SelectObject(pDC, brOld);
    DeleteObject(brImage);
	return 1;
}
#endif //CXIMAGE_SUPPORT_WINCE
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
#if CXIMAGE_SUPPORT_WINDOWS
////////////////////////////////////////////////////////////////////////////////
/**
 * Transfer the image in a global bitmap handle (clipboard copy)
 */
HANDLE CxImage::CopyToHandle()
{
	HANDLE hMem=NULL;
	if (pDib){
		hMem= GlobalAlloc(GHND, GetSize());
		if (hMem){
			BYTE* pDst=(BYTE*)GlobalLock(hMem);
			if (pDst){
				memcpy(pDst,pDib,GetSize());
			}
			GlobalUnlock(hMem);
		}
	}
	return hMem;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Global object (clipboard paste) constructor
 * \param hMem: source bitmap object, the clipboard format must be CF_DIB
 * \return true if everything is ok
 */
bool CxImage::CreateFromHANDLE(HANDLE hMem)
{
	if (!Destroy())
		return false;

	DWORD dwSize = GlobalSize(hMem);
	if (!dwSize) return false;

	BYTE *lpVoid;						//pointer to the bitmap
	lpVoid = (BYTE *)GlobalLock(hMem);
	BITMAPINFOHEADER *pHead;			//pointer to the bitmap header
	pHead = (BITMAPINFOHEADER *)lpVoid;
	if (lpVoid){

		//CxMemFile hFile(lpVoid,dwSize);

		//copy the bitmap header
		memcpy(&head,pHead,sizeof(BITMAPINFOHEADER));
		//create the image
		if(!Create(head.biWidth,head.biHeight,head.biBitCount)){
			GlobalUnlock(lpVoid);
			return false;
		}
		//preserve DPI
		if (head.biXPelsPerMeter) SetXDPI((long)floor(head.biXPelsPerMeter * 254.0 / 10000.0 + 0.5)); else SetXDPI(96);
		if (head.biYPelsPerMeter) SetYDPI((long)floor(head.biYPelsPerMeter * 254.0 / 10000.0 + 0.5)); else SetYDPI(96);

		/*//copy the pixels (old way)
		if((pHead->biCompression != BI_RGB) || (pHead->biBitCount == 32)){ //<Jörgen Alfredsson>
			// BITFIELD case
			// set the internal header in the dib
			memcpy(pDib,&head,sizeof(head));
			// get the bitfield masks
			DWORD bf[3];
			memcpy(bf,lpVoid+pHead->biSize,12);
			// transform into RGB
			Bitfield2RGB(lpVoid+pHead->biSize+12,(WORD)bf[0],(WORD)bf[1],(WORD)bf[2],(BYTE)pHead->biBitCount);
		} else { //normal bitmap
			memcpy(pDib,lpVoid,GetSize());
		}*/

		// <Michael Gandyra>
		// fill in color map
		bool bIsOldBmp = (head.biSize == sizeof(BITMAPCOREHEADER));
		RGBQUAD *pRgb = GetPalette();
		if (pRgb) {
			// number of colors to fill in
			int nColors = DibNumColors(pHead);
			if (bIsOldBmp) {
				/* get pointer to BITMAPCOREINFO (old style 1.x) */
				LPBITMAPCOREINFO lpbmc = (LPBITMAPCOREINFO)lpVoid;
				for (int i = nColors - 1; i >= 0; i--) {
					pRgb[i].rgbRed      = lpbmc->bmciColors[i].rgbtRed;
					pRgb[i].rgbGreen    = lpbmc->bmciColors[i].rgbtGreen;
					pRgb[i].rgbBlue     = lpbmc->bmciColors[i].rgbtBlue;
					pRgb[i].rgbReserved = (BYTE)0;
				}
			} else {
				/* get pointer to BITMAPINFO (new style 3.x) */
				LPBITMAPINFO lpbmi = (LPBITMAPINFO)lpVoid;
				for (int i = nColors - 1; i >= 0; i--) {
					pRgb[i].rgbRed      = lpbmi->bmiColors[i].rgbRed;
					pRgb[i].rgbGreen    = lpbmi->bmiColors[i].rgbGreen;
					pRgb[i].rgbBlue     = lpbmi->bmiColors[i].rgbBlue;
					pRgb[i].rgbReserved = (BYTE)0;
				}
			}
		}

		// <Michael Gandyra>
		DWORD dwCompression = pHead->biCompression;
		// compressed bitmap ?
		if(dwCompression!=BI_RGB || pHead->biBitCount==32) {
			// get the bitmap bits
			LPSTR lpDIBBits = (LPSTR)((BYTE*)pHead + *(DWORD*)pHead + (WORD)(GetNumColors() * sizeof(RGBQUAD)));
			// decode and copy them to our image
			switch (pHead->biBitCount) {
			case 32 :
				{
					// BITFIELD case
					if (dwCompression == BI_BITFIELDS || dwCompression == BI_RGB) {
						// get the bitfield masks
						DWORD bf[3];
						memcpy(bf,lpVoid+pHead->biSize,12);
						// transform into RGB
						Bitfield2RGB(lpVoid+pHead->biSize+12,(WORD)bf[0],(WORD)bf[1],(WORD)bf[2],(BYTE)pHead->biBitCount);
					} else 
						throw "unknown compression";
				}
				break;
			case 16 :
				{
					// get the bitfield masks
					long offset=0;
					DWORD bf[3];
					if (dwCompression == BI_BITFIELDS) {
						memcpy(bf,lpVoid+pHead->biSize,12);
						offset= 12;
					} else {
						bf[0] = 0x7C00;
						bf[1] = 0x3E0;
						bf[2] = 0x1F; // RGB555
					}
					// copy the pixels
					memcpy(info.pImage, lpDIBBits + offset, head.biHeight*((head.biWidth+1)/2)*4);
					// transform into RGB
					Bitfield2RGB(info.pImage, (WORD)bf[0], (WORD)bf[1], (WORD)bf[2], 16);
				}
				break;
			case 8 :
			case 4 :
			case 1 :
				{
					switch (dwCompression) {
					case BI_RLE4:
						{
							BYTE status_byte = 0;
							BYTE second_byte = 0;
							int scanline = 0;
							int bits = 0;
							BOOL low_nibble = FALSE;
							CImageIterator iter(this);

							for (BOOL bContinue = TRUE; bContinue; ) {
								status_byte = *(lpDIBBits++);
								switch (status_byte) {
								case RLE_COMMAND :
									status_byte = *(lpDIBBits++);
									switch (status_byte) {
									case RLE_ENDOFLINE :
										bits = 0;
										scanline++;
										low_nibble = FALSE;
										break;
									case RLE_ENDOFBITMAP :
										bContinue = FALSE;
										break;
									case RLE_DELTA :
										{
											// read the delta values
											BYTE delta_x;
											BYTE delta_y;
											delta_x = *(lpDIBBits++);
											delta_y = *(lpDIBBits++);
											// apply them
											bits       += delta_x / 2;
											scanline   += delta_y;
											break;
										}
									default :
										second_byte = *(lpDIBBits++);
										BYTE* sline = iter.GetRow(scanline);
										for (int i = 0; i < status_byte; i++) {
											if (low_nibble) {
												if ((DWORD)(sline + bits) < (DWORD)(info.pImage + head.biSizeImage)) {
													*(sline + bits) |=  second_byte & 0x0F;
												}
												if (i != status_byte - 1)
													second_byte = *(lpDIBBits++);
												bits++;
											} else {
												if ((DWORD)(sline + bits) <(DWORD)(info.pImage + head.biSizeImage)) {
													*(sline + bits) = (BYTE)(second_byte & 0xF0);
												}
											}
											low_nibble = !low_nibble;
										}
										if ((((status_byte+1) >> 1) & 1 )== 1)
											second_byte = *(lpDIBBits++);
										break;
									};
									break;
									default :
										{
											BYTE* sline = iter.GetRow(scanline);
											second_byte = *(lpDIBBits++);
											for (unsigned i = 0; i < status_byte; i++) {
												if (low_nibble) {
													if ((DWORD)(sline + bits) <(DWORD)(info.pImage + head.biSizeImage)) {
														*(sline + bits) |= second_byte & 0x0F;
													}
													bits++;
												} else {
													if ((DWORD)(sline + bits) <(DWORD)(info.pImage + head.biSizeImage))	{
														*(sline + bits) = (BYTE)(second_byte & 0xF0);
													}
												}
												low_nibble = !low_nibble;
											}
										}
										break;
								};
							}
						}
						break;
					case BI_RLE8 :
						{
							BYTE status_byte = 0;
							BYTE second_byte = 0;
							int scanline = 0;
							int bits = 0;
							CImageIterator iter(this);

							for (BOOL bContinue = TRUE; bContinue; ) {
								status_byte = *(lpDIBBits++);
								if (status_byte==RLE_COMMAND) {
									status_byte = *(lpDIBBits++);
									switch (status_byte) {
									case RLE_ENDOFLINE :
										bits = 0;
										scanline++;
										break;
									case RLE_ENDOFBITMAP :
										bContinue = FALSE;
										break;
									case RLE_DELTA :
										{
											// read the delta values
											BYTE delta_x;
											BYTE delta_y;
											delta_x = *(lpDIBBits++);
											delta_y = *(lpDIBBits++);
											// apply them
											bits     += delta_x;
											scanline += delta_y;
										}
										break;
									default :
										int nNumBytes = sizeof(BYTE) * status_byte;
										memcpy((void *)(iter.GetRow(scanline) + bits), lpDIBBits, nNumBytes);
										lpDIBBits += nNumBytes;
										// align run length to even number of bytes 
										if ((status_byte & 1) == 1)
											second_byte = *(lpDIBBits++);
										bits += status_byte;
										break;
									};
								} else {
									BYTE *sline = iter.GetRow(scanline);
									second_byte = *(lpDIBBits++);
									for (unsigned i = 0; i < status_byte; i++) {
										if ((DWORD)bits<info.dwEffWidth){
											*(sline + bits) = second_byte;
											bits++;
										} else {
											bContinue = FALSE;
											break;
										}
									}
								}
							}
						}
						break;
					default :
						throw "compression type not supported";
					}
				}
			}
		} else {
			//normal bitmap (not compressed)
			memcpy(pDib,lpVoid,GetSize());
		}

		GlobalUnlock(lpVoid);
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Transfer the image in a  bitmap handle
 * \param hdc: target device context (the screen, usually)
 * \return bitmap handle, or NULL if an error occurs.
 */
HBITMAP CxImage::MakeBitmap(HDC hdc)
{
	if (!pDib)
		return NULL;

	if (!hdc){
		// this call to CreateBitmap doesn't create a DIB <jaslet>
		// // Create a device-independent bitmap <CSC>
		//  return CreateBitmap(head.biWidth,head.biHeight,	1, head.biBitCount, GetBits());
		// use instead this code
		HDC hMemDC = CreateCompatibleDC(NULL);
		LPVOID pBit32;
		HBITMAP bmp = CreateDIBSection(hMemDC,(LPBITMAPINFO)pDib,DIB_RGB_COLORS, &pBit32, NULL, 0);
		if (pBit32) memcpy(pBit32, GetBits(), head.biSizeImage);
		DeleteDC(hMemDC);
		return bmp;
	}

	// this single line seems to work very well
	HBITMAP bmp = CreateDIBitmap(hdc, (LPBITMAPINFOHEADER)pDib, CBM_INIT,
		GetBits(), (LPBITMAPINFO)pDib, DIB_RGB_COLORS);

	return bmp;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Bitmap resource constructor
 * \param hbmp : bitmap resource handle
 * \param hpal : (optional) palette, useful for 8bpp DC 
 * \return true if everything is ok
 */
bool CxImage::CreateFromHBITMAP(HBITMAP hbmp, HPALETTE hpal)
{
	if (!Destroy())
		return false;

	if (hbmp) { 
        BITMAP bm;
		// get informations about the bitmap
        GetObject(hbmp, sizeof(BITMAP), (LPSTR) &bm);
		// create the image
        if (!Create(bm.bmWidth, bm.bmHeight, bm.bmBitsPixel, 0))
			return false;
		// create a device context for the bitmap
        HDC dc = ::GetDC(NULL);
		if (!dc)
			return false;

		if (hpal){
			SelectObject(dc,hpal); //the palette you should get from the user or have a stock one
			RealizePalette(dc);
		}

		// copy the pixels
        if (GetDIBits(dc, hbmp, 0, head.biHeight, info.pImage,
			(LPBITMAPINFO)pDib, DIB_RGB_COLORS) == 0){ //replace &head with pDib <Wil Stark>
            strcpy(info.szLastError,"GetDIBits failed");
			::ReleaseDC(NULL, dc);
			return false;
        }
        ::ReleaseDC(NULL, dc);
		return true;
    }
	return false;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * icon resource constructor
 * \param hico : icon resource handle
 * \return true if everything is ok
 */
bool CxImage::CreateFromHICON(HICON hico)
{
	if (!Destroy())
		return false;

	if (hico) { 
		ICONINFO iinfo;
		GetIconInfo(hico,&iinfo);
		if (!CreateFromHBITMAP(iinfo.hbmColor))
			return false;
#if CXIMAGE_SUPPORT_ALPHA
		CxImage mask;
		mask.CreateFromHBITMAP(iinfo.hbmMask);
		mask.GrayScale();
		mask.Negative();
		AlphaSet(mask);
#endif
		DeleteObject(iinfo.hbmColor); //<Sims>
		DeleteObject(iinfo.hbmMask);  //<Sims>
		return true;
    }
	return false;
}
////////////////////////////////////////////////////////////////////////////////
long CxImage::Draw(HDC hdc, const RECT& rect, RECT* pClipRect, bool bSmooth)
{
	return Draw(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, pClipRect,bSmooth);
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Draws the image in the specified device context, with support for alpha channel, alpha palette, transparency, opacity.
 * \param hdc : destination device context
 * \param x,y : (optional) offset
 * \param cx,cy : (optional) size.
 *                 - If cx or cy are not specified (or less than 0), the normal width or height will be used
 *                 - If cx or cy are different than width or height, the image will be stretched
 *
 * \param pClipRect : limit the drawing operations inside a given rectangle in the output device context.
 * \param bSmooth : activates a bilinear filter that will enhance the appearence for zommed pictures.
 *                   Quite slow. Needs CXIMAGE_SUPPORT_INTERPOLATION.
 * \return true if everything is ok
 */
long CxImage::Draw(HDC hdc, long x, long y, long cx, long cy, RECT* pClipRect, bool bSmooth)
{
	if((pDib==0)||(hdc==0)||(cx==0)||(cy==0)||(!info.bEnabled)) return 0;

	if (cx < 0) cx = head.biWidth;
	if (cy < 0) cy = head.biHeight;
	bool bTransparent = info.nBkgndIndex != -1;
	bool bAlpha = pAlpha != 0;

	RECT mainbox; // (experimental) 
	if (pClipRect){
		GetClipBox(hdc,&mainbox);
		HRGN rgn = CreateRectRgnIndirect(pClipRect);
		ExtSelectClipRgn(hdc,rgn,RGN_AND);
		DeleteObject(rgn);
	}

	//find the smallest area to paint
	RECT clipbox,paintbox;
	GetClipBox(hdc,&clipbox);

	paintbox.top = min(clipbox.bottom,max(clipbox.top,y));
	paintbox.left = min(clipbox.right,max(clipbox.left,x));
	paintbox.right = max(clipbox.left,min(clipbox.right,x+cx));
	paintbox.bottom = max(clipbox.top,min(clipbox.bottom,y+cy));

	long destw = paintbox.right - paintbox.left;
	long desth = paintbox.bottom - paintbox.top;

	if (!(bTransparent || bAlpha || info.bAlphaPaletteEnabled)){
		if (cx==head.biWidth && cy==head.biHeight){ //NORMAL
			SetStretchBltMode(hdc,COLORONCOLOR);	
			SetDIBitsToDevice(hdc, x, y, cx, cy, 0, 0, 0, cy,
						info.pImage,(BITMAPINFO*)pDib,DIB_RGB_COLORS);
		} else { //STRETCH
			//pixel informations
			RGBQUAD c={0,0,0,0};
			//Preparing Bitmap Info
			BITMAPINFO bmInfo;
			memset(&bmInfo.bmiHeader,0,sizeof(BITMAPINFOHEADER));
			bmInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
			bmInfo.bmiHeader.biWidth=destw;
			bmInfo.bmiHeader.biHeight=desth;
			bmInfo.bmiHeader.biPlanes=1;
			bmInfo.bmiHeader.biBitCount=24;
			BYTE *pbase;	//points to the final dib
			BYTE *pdst;		//current pixel from pbase
			BYTE *ppix;		//current pixel from image
			//get the background
			HDC TmpDC=CreateCompatibleDC(hdc);
			HBITMAP TmpBmp=CreateDIBSection(hdc,&bmInfo,DIB_RGB_COLORS,(void**)&pbase,0,0);
			HGDIOBJ TmpObj=SelectObject(TmpDC,TmpBmp);

			if (pbase){
				long xx,yy;
				long sx,sy;
				float dx,dy;
				BYTE *psrc;

				long ew = ((((24 * destw) + 31) / 32) * 4);
				long ymax = paintbox.bottom;
				long xmin = paintbox.left;
				float fx=(float)head.biWidth/(float)cx;
				float fy=(float)head.biHeight/(float)cy;

				for(yy=0;yy<desth;yy++){
					dy = head.biHeight-(ymax-yy-y)*fy;
					sy = max(0L,(long)floor(dy));
					psrc = info.pImage+sy*info.dwEffWidth;
					pdst = pbase+yy*ew;
					for(xx=0;xx<destw;xx++){
						dx = (xx+xmin-x)*fx;
						sx = max(0L,(long)floor(dx));
#if CXIMAGE_SUPPORT_INTERPOLATION
						if (bSmooth){
							c = GetPixelColorInterpolated(dx - 0.5f, dy - 0.5f, CxImage::IM_BILINEAR, CxImage::OM_REPEAT);
						} else
#endif //CXIMAGE_SUPPORT_INTERPOLATION
						{
							if (head.biClrUsed){
								c=GetPaletteColor(GetPixelIndex(sx,sy));
							} else {
								ppix = psrc + sx*3;
								c.rgbBlue = *ppix++;
								c.rgbGreen= *ppix++;
								c.rgbRed  = *ppix;
							}
						}
						*pdst++=c.rgbBlue;
						*pdst++=c.rgbGreen;
						*pdst++=c.rgbRed;
					}
				}
			}
			//paint the image & cleanup
			SetDIBitsToDevice(hdc,paintbox.left,paintbox.top,destw,desth,0,0,0,desth,pbase,&bmInfo,0);
			DeleteObject(SelectObject(TmpDC,TmpObj));
			DeleteDC(TmpDC);
		}
	} else {	// draw image with transparent/alpha blending
	//////////////////////////////////////////////////////////////////
		//Alpha blend - Thanks to Florian Egel

		//pixel informations
		RGBQUAD c={0,0,0,0};
		RGBQUAD ct = GetTransColor();
		long* pc = (long*)&c;
		long* pct= (long*)&ct;
		long cit = GetTransIndex();
		long ci;

		//Preparing Bitmap Info
		BITMAPINFO bmInfo;
		memset(&bmInfo.bmiHeader,0,sizeof(BITMAPINFOHEADER));
		bmInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
		bmInfo.bmiHeader.biWidth=destw;
		bmInfo.bmiHeader.biHeight=desth;
		bmInfo.bmiHeader.biPlanes=1;
		bmInfo.bmiHeader.biBitCount=24;

		BYTE *pbase;	//points to the final dib
		BYTE *pdst;		//current pixel from pbase
		BYTE *ppix;		//current pixel from image

		//get the background
		HDC TmpDC=CreateCompatibleDC(hdc);
		HBITMAP TmpBmp=CreateDIBSection(hdc,&bmInfo,DIB_RGB_COLORS,(void**)&pbase,0,0);
		HGDIOBJ TmpObj=SelectObject(TmpDC,TmpBmp);
		BitBlt(TmpDC,0,0,destw,desth,hdc,paintbox.left,paintbox.top,SRCCOPY);

		if (pbase){
			long xx,yy,alphaoffset,ix,iy;
			BYTE a,a1,*psrc;
			long ew = ((((24 * destw) + 31) / 32) * 4);
			long ymax = paintbox.bottom;
			long xmin = paintbox.left;

			if (cx!=head.biWidth || cy!=head.biHeight){
				//STRETCH
				float fx=(float)head.biWidth/(float)cx;
				float fy=(float)head.biHeight/(float)cy;
				float dx,dy;
				long sx,sy;
				
				for(yy=0;yy<desth;yy++){
					dy = head.biHeight-(ymax-yy-y)*fy;
					sy = max(0L,(long)floor(dy));

					alphaoffset = sy*head.biWidth;
					pdst = pbase + yy*ew;
					psrc = info.pImage + sy*info.dwEffWidth;

					for(xx=0;xx<destw;xx++){
						dx = (xx+xmin-x)*fx;
						sx = max(0L,(long)floor(dx));

						if (bAlpha) a=pAlpha[alphaoffset+sx]; else a=255;
						a =(BYTE)((a*(1+info.nAlphaMax))>>8);

						if (head.biClrUsed){
							ci = GetPixelIndex(sx,sy);
#if CXIMAGE_SUPPORT_INTERPOLATION
							if (bSmooth){
								c = GetPixelColorInterpolated(dx - 0.5f, dy - 0.5f, CxImage::IM_BILINEAR, CxImage::OM_REPEAT);
							} else
#endif //CXIMAGE_SUPPORT_INTERPOLATION
							{
								c = GetPaletteColor(GetPixelIndex(sx,sy));
							}
							if (info.bAlphaPaletteEnabled){
								a = (BYTE)((a*(1+c.rgbReserved))>>8);
							}
						} else {
#if CXIMAGE_SUPPORT_INTERPOLATION
							if (bSmooth){
								c = GetPixelColorInterpolated(dx - 0.5f, dy - 0.5f, CxImage::IM_BILINEAR, CxImage::OM_REPEAT);
							} else
#endif //CXIMAGE_SUPPORT_INTERPOLATION
							{
								ppix = psrc + sx*3;
								c.rgbBlue = *ppix++;
								c.rgbGreen= *ppix++;
								c.rgbRed  = *ppix;
							}
						}
						//if (*pc!=*pct || !bTransparent){
						//if ((head.biClrUsed && ci!=cit) || ((!head.biClrUsed||bSmooth) && *pc!=*pct) || !bTransparent){
						if ((head.biClrUsed && ci!=cit) || (!head.biClrUsed && *pc!=*pct) || !bTransparent){
							// DJT, assume many pixels are fully transparent or opaque and thus avoid multiplication
							if (a == 0) {			// Transparent, retain dest 
								pdst+=3; 
							} else if (a == 255) {	// opaque, ignore dest 
								*pdst++= c.rgbBlue; 
								*pdst++= c.rgbGreen; 
								*pdst++= c.rgbRed; 
							} else {				// semi transparent 
								a1=(BYTE)~a;
								*pdst++=(BYTE)((*pdst * a1 + a * c.rgbBlue)>>8); 
								*pdst++=(BYTE)((*pdst * a1 + a * c.rgbGreen)>>8); 
								*pdst++=(BYTE)((*pdst * a1 + a * c.rgbRed)>>8); 
							} 
						} else {
							pdst+=3;
						}
					}
				}
			} else {
				//NORMAL
				iy=head.biHeight-ymax+y;
				for(yy=0;yy<desth;yy++,iy++){
					alphaoffset=iy*head.biWidth;
					ix=xmin-x;
					pdst=pbase+yy*ew;
					ppix=info.pImage+iy*info.dwEffWidth+ix*3;
					for(xx=0;xx<destw;xx++,ix++){

						if (bAlpha) a=pAlpha[alphaoffset+ix]; else a=255;
						a = (BYTE)((a*(1+info.nAlphaMax))>>8);

						if (head.biClrUsed){
							ci = GetPixelIndex(ix,iy);
							c = GetPaletteColor((BYTE)ci);
							if (info.bAlphaPaletteEnabled){
								a = (BYTE)((a*(1+c.rgbReserved))>>8);
							}
						} else {
							c.rgbBlue = *ppix++;
							c.rgbGreen= *ppix++;
							c.rgbRed  = *ppix++;
						}

						//if (*pc!=*pct || !bTransparent){
						if ((head.biClrUsed && ci!=cit) || (!head.biClrUsed && *pc!=*pct) || !bTransparent){
							// DJT, assume many pixels are fully transparent or opaque and thus avoid multiplication
							if (a == 0) {			// Transparent, retain dest 
								pdst+=3; 
							} else if (a == 255) {	// opaque, ignore dest 
								*pdst++= c.rgbBlue; 
								*pdst++= c.rgbGreen; 
								*pdst++= c.rgbRed; 
							} else {				// semi transparent 
								a1=(BYTE)~a;
								*pdst++=(BYTE)((*pdst * a1 + a * c.rgbBlue)>>8); 
								*pdst++=(BYTE)((*pdst * a1 + a * c.rgbGreen)>>8); 
								*pdst++=(BYTE)((*pdst * a1 + a * c.rgbRed)>>8); 
							} 
						} else {
							pdst+=3;
						}
					}
				}
			}
		}
		//paint the image & cleanup
		SetDIBitsToDevice(hdc,paintbox.left,paintbox.top,destw,desth,0,0,0,desth,pbase,&bmInfo,0);
		DeleteObject(SelectObject(TmpDC,TmpObj));
		DeleteDC(TmpDC);
	}

	if (pClipRect){  // (experimental)
		HRGN rgn = CreateRectRgnIndirect(&mainbox);
		ExtSelectClipRgn(hdc,rgn,RGN_OR);
		DeleteObject(rgn);
	}

	return 1;
}
////////////////////////////////////////////////////////////////////////////////
long CxImage::Draw2(HDC hdc, const RECT& rect)
{
	return Draw2(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Draws (stretch) the image with single transparency support
 * \param hdc : destination device context
 * \param x,y : (optional) offset
 * \param cx,cy : (optional) size.
 *                 - If cx or cy are not specified (or less than 0), the normal width or height will be used
 *                 - If cx or cy are different than width or height, the image will be stretched
 *
 * \return true if everything is ok
 */
long CxImage::Draw2(HDC hdc, long x, long y, long cx, long cy)
{
	if((pDib==0)||(hdc==0)||(cx==0)||(cy==0)||(!info.bEnabled)) return 0;
	if (cx < 0) cx = head.biWidth;
	if (cy < 0) cy = head.biHeight;
	bool bTransparent = (info.nBkgndIndex != -1);

	if (!bTransparent){
		SetStretchBltMode(hdc,COLORONCOLOR);	
		StretchDIBits(hdc, x, y, cx, cy, 0, 0, head.biWidth, head.biHeight,
						info.pImage,(BITMAPINFO*)pDib, DIB_RGB_COLORS,SRCCOPY);
	} else {
		// draw image with transparent background
		const int safe = 0; // or else GDI fails in the following - sometimes 
		RECT rcDst = {x+safe, y+safe, x+cx, y+cy};
		if (RectVisible(hdc, &rcDst)){
		/////////////////////////////////////////////////////////////////
			// True Mask Method - Thanks to Paul Reynolds and Ron Gery
			int nWidth = head.biWidth;
			int nHeight = head.biHeight;
			// Create two memory dcs for the image and the mask
			HDC dcImage=CreateCompatibleDC(hdc);
			HDC dcTrans=CreateCompatibleDC(hdc);
			// Select the image into the appropriate dc
			HBITMAP bm = CreateCompatibleBitmap(hdc, nWidth, nHeight);
			HBITMAP pOldBitmapImage = (HBITMAP)SelectObject(dcImage,bm);
			SetStretchBltMode(dcImage,COLORONCOLOR);
			StretchDIBits(dcImage, 0, 0, nWidth, nHeight, 0, 0, nWidth, nHeight,
							info.pImage,(BITMAPINFO*)pDib,DIB_RGB_COLORS,SRCCOPY);

			// Create the mask bitmap
			HBITMAP bitmapTrans = CreateBitmap(nWidth, nHeight, 1, 1, NULL);
			// Select the mask bitmap into the appropriate dc
			HBITMAP pOldBitmapTrans = (HBITMAP)SelectObject(dcTrans, bitmapTrans);
			// Build mask based on transparent colour
			RGBQUAD rgbBG;
			if (head.biBitCount<24) rgbBG = GetPaletteColor((BYTE)info.nBkgndIndex);
			else rgbBG = info.nBkgndColor;
			COLORREF crColour = RGB(rgbBG.rgbRed, rgbBG.rgbGreen, rgbBG.rgbBlue);
			COLORREF crOldBack = SetBkColor(dcImage,crColour);
			BitBlt(dcTrans,0, 0, nWidth, nHeight, dcImage, 0, 0, SRCCOPY);

			// Do the work - True Mask method - cool if not actual display
			StretchBlt(hdc,x, y,cx,cy, dcImage, 0, 0, nWidth, nHeight, SRCINVERT);
			StretchBlt(hdc,x, y,cx,cy, dcTrans, 0, 0, nWidth, nHeight, SRCAND);
			StretchBlt(hdc,x, y,cx,cy, dcImage, 0, 0, nWidth, nHeight, SRCINVERT);

			// Restore settings
			SelectObject(dcImage,pOldBitmapImage);
			SelectObject(dcTrans,pOldBitmapTrans);
			SetBkColor(hdc,crOldBack);
			DeleteObject( bitmapTrans );  // RG 29/01/2002
			DeleteDC(dcImage);
			DeleteDC(dcTrans);
			DeleteObject(bm);
		}
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////
long CxImage::Stretch(HDC hdc, const RECT& rect, DWORD dwRop)
{
	return Stretch(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, dwRop);
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Stretch the image. Obsolete: use Draw() or Draw2()
 * \param hdc : destination device context
 * \param xoffset,yoffset : (optional) offset
 * \param xsize,ysize : size.
 * \param dwRop : raster operation code (see BitBlt documentation)
 * \return true if everything is ok
 */
long CxImage::Stretch(HDC hdc, long xoffset, long yoffset, long xsize, long ysize, DWORD dwRop)
{
	if((pDib)&&(hdc)) {
		//palette must be correctly filled
		SetStretchBltMode(hdc,COLORONCOLOR);	
		StretchDIBits(hdc, xoffset, yoffset,
					xsize, ysize, 0, 0, head.biWidth, head.biHeight,
					info.pImage,(BITMAPINFO*)pDib,DIB_RGB_COLORS,dwRop);
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Tiles the device context in the specified rectangle with the image.
 * \param hdc : destination device context
 * \param rc : tiled rectangle in the output device context
 * \return true if everything is ok
 */
long CxImage::Tile(HDC hdc, RECT *rc)
{
	if((pDib)&&(hdc)&&(rc)) {
		int w = rc->right - rc->left;
		int h = rc->bottom - rc->top;
		int x,y,z;
		int bx=head.biWidth;
		int by=head.biHeight;
		for (y = 0 ; y < h ; y += by){
			if ((y+by)>h) by=h-y;
			z=bx;
			for (x = 0 ; x < w ; x += z){
				if ((x+z)>w) z=w-x;
				RECT r = {rc->left + x,rc->top + y,rc->left + x + z,rc->top + y + by};
				Draw(hdc,rc->left + x, rc->top + y,-1,-1,&r);
			}
		}
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
// For UNICODE support: char -> TCHAR
long CxImage::DrawString(HDC hdc, long x, long y, const TCHAR* text, RGBQUAD color, const TCHAR* font, long lSize, long lWeight, BYTE bItalic, BYTE bUnderline, bool bSetAlpha)
//long CxImage::DrawString(HDC hdc, long x, long y, const char* text, RGBQUAD color, const char* font, long lSize, long lWeight, BYTE bItalic, BYTE bUnderline, bool bSetAlpha)
{
	if (IsValid()){
		//get the background
		HDC TmpDC=CreateCompatibleDC(hdc);
		//choose the font
		HFONT m_Font;
		LOGFONT* m_pLF;
		m_pLF=(LOGFONT*)calloc(1,sizeof(LOGFONT));
		_tcsncpy(m_pLF->lfFaceName,font,31);	// For UNICODE support
		//strncpy(m_pLF->lfFaceName,font,31);
		m_pLF->lfHeight=lSize;
		m_pLF->lfWeight=lWeight;
		m_pLF->lfItalic=bItalic;
		m_pLF->lfUnderline=bUnderline;
		m_Font=CreateFontIndirect(m_pLF);
		//select the font in the dc
		HFONT pOldFont=NULL;
		if (m_Font)
			pOldFont = (HFONT)SelectObject(TmpDC,m_Font);
		else
			pOldFont = (HFONT)SelectObject(TmpDC,GetStockObject(DEFAULT_GUI_FONT));

		//Set text color
		SetTextColor(TmpDC,RGB(255,255,255));
		SetBkColor(TmpDC,RGB(0,0,0));
		//draw the text
		SetBkMode(TmpDC,OPAQUE);
		//Set text position;
		RECT pos = {0,0,0,0};
		//long len = (long)strlen(text);
		long len = (long)_tcslen(text);	// For UNICODE support
		::DrawText(TmpDC,text,len,&pos,DT_CALCRECT);
		pos.right+=pos.bottom; //for italics

		//Preparing Bitmap Info
		long width=pos.right;
		long height=pos.bottom;
		BITMAPINFO bmInfo;
		memset(&bmInfo.bmiHeader,0,sizeof(BITMAPINFOHEADER));
		bmInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
		bmInfo.bmiHeader.biWidth=width;
		bmInfo.bmiHeader.biHeight=height;
		bmInfo.bmiHeader.biPlanes=1;
		bmInfo.bmiHeader.biBitCount=24;
		BYTE *pbase; //points to the final dib

		HBITMAP TmpBmp=CreateDIBSection(TmpDC,&bmInfo,DIB_RGB_COLORS,(void**)&pbase,0,0);
		HGDIOBJ TmpObj=SelectObject(TmpDC,TmpBmp);
		memset(pbase,0,height*((((24 * width) + 31) / 32) * 4));

		::DrawText(TmpDC,text,len,&pos,0);

		CxImage itext;
		itext.CreateFromHBITMAP(TmpBmp);

		y=head.biHeight-y-1;
		for (long ix=0;ix<width;ix++){
			for (long iy=0;iy<height;iy++){
				if (itext.GetPixelColor(ix,iy).rgbBlue) SetPixelColor(x+ix,y+iy,color,bSetAlpha);
			}
		}

		//cleanup
		if (pOldFont) SelectObject(TmpDC,pOldFont);
		DeleteObject(m_Font);
		free(m_pLF);
		DeleteObject(SelectObject(TmpDC,TmpObj));
		DeleteDC(TmpDC);
	}

	return 1;
}
////////////////////////////////////////////////////////////////////////////////
// <VATI>
long CxImage::DrawStringEx(HDC hdc, long x, long y, CXTEXTINFO *pTextType, bool bSetAlpha )
{
	if (!IsValid())
        return -1;
    
	//get the background
	HDC pDC;
	if (hdc) pDC=hdc; else pDC = ::GetDC(0);
	HDC TmpDC=CreateCompatibleDC(pDC);
   	
    //choose the font
	HFONT m_Font;
    m_Font=CreateFontIndirect( &pTextType->lfont );
    
    // get colors in RGBQUAD
    RGBQUAD p_forecolor = RGBtoRGBQUAD(pTextType->fcolor);
    RGBQUAD p_backcolor = RGBtoRGBQUAD(pTextType->bcolor);

    // check alignment and re-set default if necessary
    if ( pTextType->align != DT_CENTER &&
         pTextType->align != DT_LEFT &&
         pTextType->align != DT_RIGHT )
        pTextType->align = DT_CENTER;

    // check rounding radius and re-set default if necessary
    if ( pTextType->b_round > 50 || pTextType->b_round < 0 )
        pTextType->b_round = 10;

    // check opacity and re-set default if necessary
    if ( pTextType->b_opacity > 1. || pTextType->b_opacity < .0 )
        pTextType->b_opacity = 0.;

    //select the font in the dc
	HFONT pOldFont=NULL;
	if (m_Font)
		pOldFont = (HFONT)SelectObject(TmpDC,m_Font);
	else
		pOldFont = (HFONT)SelectObject(TmpDC,GetStockObject(DEFAULT_GUI_FONT));

	//Set text color
    SetTextColor(TmpDC,RGB(255,255,255));
	SetBkColor(TmpDC,RGB(0,0,0));
	SetBkMode(TmpDC,OPAQUE);
	//Set text position;
	RECT pos = {0,0,0,0};
	
    // get text length and number of lines
    long i=0, numlines=1, len=(long)_tcsclen(pTextType->text);
    while (i<len)
    {
        if ( pTextType->text[i++]==13 )
            numlines++;
    }

	::DrawText(TmpDC, pTextType->text, len, &pos, /*DT_EDITCONTROL|DT_EXTERNALLEADING|*/DT_NOPREFIX | DT_CALCRECT );

    // increase only if it's really italics, and only one line height
	if ( pTextType->lfont.lfItalic ) 
        pos.right += pos.bottom/2/numlines; 

    // background frame and rounding radius
	int frame = 0, roundR = 0;
    if ( pTextType->opaque )
    {
        roundR= (int)(pos.bottom/numlines * pTextType->b_round / 100 ) ;
        frame = (int)(/*3.5 + */0.29289*roundR ) ;
        pos.right += pos.bottom/numlines/3 ; // JUST FOR BEAUTY
    }

	//Preparing Bitmap Info
	long width=pos.right +frame*2;
	long height=pos.bottom +frame*2;
	BITMAPINFO bmInfo;
	memset(&bmInfo.bmiHeader,0,sizeof(BITMAPINFOHEADER));
	bmInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	bmInfo.bmiHeader.biWidth=width;
	bmInfo.bmiHeader.biHeight=height;
	bmInfo.bmiHeader.biPlanes=1;
	bmInfo.bmiHeader.biBitCount=24;
	BYTE *pbase; //points to the final dib

	HBITMAP TmpBmp=CreateDIBSection(TmpDC,&bmInfo,DIB_RGB_COLORS,(void**)&pbase,0,0);
	HGDIOBJ TmpObj=SelectObject(TmpDC,TmpBmp);
	memset(pbase,0,height*((((24 * width) + 31) / 32) * 4));

	::DrawText(TmpDC,pTextType->text,len, &pos, /*DT_EDITCONTROL|DT_EXTERNALLEADING|*/DT_NOPREFIX| pTextType->align );
    
	CxImage itext;
	itext.CreateFromHBITMAP(TmpBmp);
    y=head.biHeight-y-1;

    //move the insertion point according to alignment type
    // DT_CENTER: cursor points to the center of text rectangle
    // DT_RIGHT:  cursor points to right side end of text rectangle
    // DT_LEFT:   cursor points to left end of text rectangle
    if ( pTextType->align == DT_CENTER )
        x -= width/2;
    else if ( pTextType->align == DT_RIGHT )
        x -= width;
    if (x<0) x=0;
    
    //draw the background first, if it exists
    long ix,iy;
    if ( pTextType->opaque )
    {
        int ixf=0; 
        for (ix=0;ix<width;ix++)
        {
            if ( ix<=roundR )
                ixf = (int)(.5+roundR-sqrt((float)(roundR*roundR-(ix-roundR)*(ix-roundR))));
            else if ( ix>=width-roundR-1 )
                ixf = (int)(.5+roundR-sqrt((float)(roundR*roundR-(width-1-ix-roundR)*(width-1-ix-roundR))));
            else
                ixf=0;

            for (iy=0;iy<height;iy++)
            {
                if ( (ix<=roundR && ( iy > height-ixf-1 || iy < ixf )) ||
                     (ix>=width-roundR-1 && ( iy > height-ixf-1 || iy < ixf )) )
                    continue;
                else
                    if ( pTextType->b_opacity > 0.0 && pTextType->b_opacity < 1.0 )
                    {
                        RGBQUAD bcolor, pcolor;
                        // calculate a transition color from original image to background color:
                        pcolor = GetPixelColor(x+ix,y+iy);
						bcolor.rgbBlue = (unsigned char)(pTextType->b_opacity * pcolor.rgbBlue + (1.0-pTextType->b_opacity) * p_backcolor.rgbBlue );
                        bcolor.rgbRed = (unsigned char)(pTextType->b_opacity * pcolor.rgbRed + (1.0-pTextType->b_opacity) * p_backcolor.rgbRed ) ;
                        bcolor.rgbGreen = (unsigned char)(pTextType->b_opacity * pcolor.rgbGreen + (1.0-pTextType->b_opacity) * p_backcolor.rgbGreen ) ;
                        bcolor.rgbReserved = 0;
                        SetPixelColor(x+ix,y+iy,bcolor,bSetAlpha);
                    }
                    else
                        SetPixelColor(x+ix,y+iy,p_backcolor,bSetAlpha);
			}
		}
    }
	
    // draw the text itself
    for (ix=0;ix<width;ix++)
    {
		for (iy=0;iy<height;iy++)
        {
            if (itext.GetPixelColor(ix,iy).rgbBlue )
                SetPixelColor(x+ix+frame,y+iy-frame,p_forecolor,bSetAlpha);
		}
	}

	//cleanup
    if (pOldFont) SelectObject(TmpDC,pOldFont);
	DeleteObject(m_Font);
	DeleteObject(SelectObject(TmpDC,TmpObj));
	DeleteDC(TmpDC);
	return 1;
}

//////////////////////////////////////////////////////////////////////////////
void CxImage::InitTextInfo( CXTEXTINFO *txt )
{

    memset( txt, 0, sizeof(CXTEXTINFO));
    
    // LOGFONT defaults
    txt->lfont.lfHeight        = -36; 
    txt->lfont.lfCharSet       = EASTEUROPE_CHARSET; // just for Central-European users 
    txt->lfont.lfWeight        = FW_NORMAL;
    txt->lfont.lfWidth         = 0; 
    txt->lfont.lfEscapement    = 0; 
    txt->lfont.lfOrientation   = 0; 
    txt->lfont.lfItalic        = FALSE; 
    txt->lfont.lfUnderline     = FALSE; 
    txt->lfont.lfStrikeOut     = FALSE; 
    txt->lfont.lfOutPrecision  = OUT_DEFAULT_PRECIS; 
    txt->lfont.lfClipPrecision = CLIP_DEFAULT_PRECIS; 
    txt->lfont.lfQuality       = PROOF_QUALITY; 
    txt->lfont.lfPitchAndFamily= DEFAULT_PITCH | FF_DONTCARE ; 
    _stprintf( txt->lfont.lfFaceName, _T("Arial")); //use TCHAR mappings <Cesar M>

    // initial colors
    txt->fcolor = RGB( 255,255,160 );  // default foreground: light goldyellow
    txt->bcolor = RGB( 32, 96, 0 );    // default background: deep green

    // background
    txt->opaque    = TRUE;  // text has a non-transparent background;
    txt->b_opacity = 0.0;   // default: opaque background
    txt->b_outline = 0;     // default: no outline (OUTLINE NOT IMPLEMENTED AT THIS TIME)
    txt->b_round   = 20;    // default: rounding radius is 20% of the rectangle height
    // the text 
    _stprintf( txt->text, _T("Sample Text 01234õû")); // text use TCHAR mappings <Cesar M>
    txt->align = DT_CENTER;
    return;
}
////////////////////////////////////////////////////////////////////////////////
#endif //CXIMAGE_SUPPORT_WINDOWS
////////////////////////////////////////////////////////////////////////////////
