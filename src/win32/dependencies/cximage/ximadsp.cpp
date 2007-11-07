// xImaDsp.cpp : DSP functions
/* 07/08/2001 v1.00 - Davide Pizzolato - www.xdp.it
 * CxImage version 5.99c 17/Oct/2004
 */

#include "ximage.h"

#include "ximaiter.h"

#if CXIMAGE_SUPPORT_DSP

////////////////////////////////////////////////////////////////////////////////
/**
 * Converts the image to B&W.
 * The Mean() function can be used for calculating the optimal threshold.
 * \param level: the lightness threshold.
 * \return true if everything is ok
 */
bool CxImage::Threshold(BYTE level)
{
	if (!pDib) return false;
	if (head.biBitCount == 1) return true;

	GrayScale();

	CxImage tmp(head.biWidth,head.biHeight,1);
	if (!tmp.IsValid()) return false;

	for (long y=0;y<head.biHeight;y++){
		info.nProgress = (long)(100*y/head.biHeight);
		if (info.nEscape) break;
		for (long x=0;x<head.biWidth;x++){
			if (GetPixelIndex(x,y)>level)
				tmp.SetPixelIndex(x,y,1);
			else
				tmp.SetPixelIndex(x,y,0);
		}
	}
	tmp.SetPaletteColor(0,0,0,0);
	tmp.SetPaletteColor(1,255,255,255);
	Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Extract RGB channels from the image. Each channel is an 8 bit grayscale image. 
 * \param r,g,b: pointers to CxImage objects, to store the splited channels
 * \return true if everything is ok
 */
bool CxImage::SplitRGB(CxImage* r,CxImage* g,CxImage* b)
{
	if (!pDib) return false;
	if (r==NULL && g==NULL && b==NULL) return false;

	CxImage tmpr(head.biWidth,head.biHeight,8);
	CxImage tmpg(head.biWidth,head.biHeight,8);
	CxImage tmpb(head.biWidth,head.biHeight,8);

	RGBQUAD color;
	for(long y=0; y<head.biHeight; y++){
		for(long x=0; x<head.biWidth; x++){
			color = GetPixelColor(x,y);
			if (r) tmpr.SetPixelIndex(x,y,color.rgbRed);
			if (g) tmpg.SetPixelIndex(x,y,color.rgbGreen);
			if (b) tmpb.SetPixelIndex(x,y,color.rgbBlue);
		}
	}

	if (r) tmpr.SetGrayPalette();
	if (g) tmpg.SetGrayPalette();
	if (b) tmpb.SetGrayPalette();

	/*for(long j=0; j<256; j++){
		BYTE i=(BYTE)j;
		if (r) tmpr.SetPaletteColor(i,i,0,0);
		if (g) tmpg.SetPaletteColor(i,0,i,0);
		if (b) tmpb.SetPaletteColor(i,0,0,i);
	}*/

	if (r) r->Transfer(tmpr);
	if (g) g->Transfer(tmpg);
	if (b) b->Transfer(tmpb);

	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Extract CMYK channels from the image. Each channel is an 8 bit grayscale image. 
 * \param c,m,y,k: pointers to CxImage objects, to store the splited channels
 * \return true if everything is ok
 */
bool CxImage::SplitCMYK(CxImage* c,CxImage* m,CxImage* y,CxImage* k)
{
	if (!pDib) return false;
	if (c==NULL && m==NULL && y==NULL && k==NULL) return false;

	CxImage tmpc(head.biWidth,head.biHeight,8);
	CxImage tmpm(head.biWidth,head.biHeight,8);
	CxImage tmpy(head.biWidth,head.biHeight,8);
	CxImage tmpk(head.biWidth,head.biHeight,8);

	RGBQUAD color;
	for(long yy=0; yy<head.biHeight; yy++){
		for(long xx=0; xx<head.biWidth; xx++){
			color = GetPixelColor(xx,yy);
			if (c) tmpc.SetPixelIndex(xx,yy,(BYTE)(255-color.rgbRed));
			if (m) tmpm.SetPixelIndex(xx,yy,(BYTE)(255-color.rgbGreen));
			if (y) tmpy.SetPixelIndex(xx,yy,(BYTE)(255-color.rgbBlue));
			if (k) tmpk.SetPixelIndex(xx,yy,(BYTE)RGB2GRAY(color.rgbRed,color.rgbGreen,color.rgbBlue));
		}
	}

	if (c) tmpc.SetGrayPalette();
	if (m) tmpm.SetGrayPalette();
	if (y) tmpy.SetGrayPalette();
	if (k) tmpk.SetGrayPalette();

	if (c) c->Transfer(tmpc);
	if (m) m->Transfer(tmpm);
	if (y) y->Transfer(tmpy);
	if (k) k->Transfer(tmpk);

	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Extract YUV channels from the image. Each channel is an 8 bit grayscale image. 
 * \param y,u,v: pointers to CxImage objects, to store the splited channels
 * \return true if everything is ok
 */
bool CxImage::SplitYUV(CxImage* y,CxImage* u,CxImage* v)
{
	if (!pDib) return false;
	if (y==NULL && u==NULL && v==NULL) return false;

	CxImage tmpy(head.biWidth,head.biHeight,8);
	CxImage tmpu(head.biWidth,head.biHeight,8);
	CxImage tmpv(head.biWidth,head.biHeight,8);

	RGBQUAD color;
	for(long yy=0; yy<head.biHeight; yy++){
		for(long x=0; x<head.biWidth; x++){
			color = RGBtoYUV(GetPixelColor(x,yy));
			if (y) tmpy.SetPixelIndex(x,yy,color.rgbRed);
			if (u) tmpu.SetPixelIndex(x,yy,color.rgbGreen);
			if (v) tmpv.SetPixelIndex(x,yy,color.rgbBlue);
		}
	}

	if (y) tmpy.SetGrayPalette();
	if (u) tmpu.SetGrayPalette();
	if (v) tmpv.SetGrayPalette();

	if (y) y->Transfer(tmpy);
	if (u) u->Transfer(tmpu);
	if (v) v->Transfer(tmpv);

	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Extract YIQ channels from the image. Each channel is an 8 bit grayscale image. 
 * \param y,i,q: pointers to CxImage objects, to store the splited channels
 * \return true if everything is ok
 */
bool CxImage::SplitYIQ(CxImage* y,CxImage* i,CxImage* q)
{
	if (!pDib) return false;
	if (y==NULL && i==NULL && q==NULL) return false;

	CxImage tmpy(head.biWidth,head.biHeight,8);
	CxImage tmpi(head.biWidth,head.biHeight,8);
	CxImage tmpq(head.biWidth,head.biHeight,8);

	RGBQUAD color;
	for(long yy=0; yy<head.biHeight; yy++){
		for(long x=0; x<head.biWidth; x++){
			color = RGBtoYIQ(GetPixelColor(x,yy));
			if (y) tmpy.SetPixelIndex(x,yy,color.rgbRed);
			if (i) tmpi.SetPixelIndex(x,yy,color.rgbGreen);
			if (q) tmpq.SetPixelIndex(x,yy,color.rgbBlue);
		}
	}

	if (y) tmpy.SetGrayPalette();
	if (i) tmpi.SetGrayPalette();
	if (q) tmpq.SetGrayPalette();

	if (y) y->Transfer(tmpy);
	if (i) i->Transfer(tmpi);
	if (q) q->Transfer(tmpq);

	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Extract XYZ channels from the image. Each channel is an 8 bit grayscale image. 
 * \param x,y,z: pointers to CxImage objects, to store the splited channels
 * \return true if everything is ok
 */
bool CxImage::SplitXYZ(CxImage* x,CxImage* y,CxImage* z)
{
	if (!pDib) return false;
	if (x==NULL && y==NULL && z==NULL) return false;

	CxImage tmpx(head.biWidth,head.biHeight,8);
	CxImage tmpy(head.biWidth,head.biHeight,8);
	CxImage tmpz(head.biWidth,head.biHeight,8);

	RGBQUAD color;
	for(long yy=0; yy<head.biHeight; yy++){
		for(long xx=0; xx<head.biWidth; xx++){
			color = RGBtoXYZ(GetPixelColor(xx,yy));
			if (x) tmpx.SetPixelIndex(xx,yy,color.rgbRed);
			if (y) tmpy.SetPixelIndex(xx,yy,color.rgbGreen);
			if (z) tmpz.SetPixelIndex(xx,yy,color.rgbBlue);
		}
	}

	if (x) tmpx.SetGrayPalette();
	if (y) tmpy.SetGrayPalette();
	if (z) tmpz.SetGrayPalette();

	if (x) x->Transfer(tmpx);
	if (y) y->Transfer(tmpy);
	if (z) z->Transfer(tmpz);

	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Extract HSL channels from the image. Each channel is an 8 bit grayscale image. 
 * \param h,s,l: pointers to CxImage objects, to store the splited channels
 * \return true if everything is ok
 */
bool CxImage::SplitHSL(CxImage* h,CxImage* s,CxImage* l)
{
	if (!pDib) return false;
	if (h==NULL && s==NULL && l==NULL) return false;

	CxImage tmph(head.biWidth,head.biHeight,8);
	CxImage tmps(head.biWidth,head.biHeight,8);
	CxImage tmpl(head.biWidth,head.biHeight,8);

	RGBQUAD color;
	for(long y=0; y<head.biHeight; y++){
		for(long x=0; x<head.biWidth; x++){
			color = RGBtoHSL(GetPixelColor(x,y));
			if (h) tmph.SetPixelIndex(x,y,color.rgbRed);
			if (s) tmps.SetPixelIndex(x,y,color.rgbGreen);
			if (l) tmpl.SetPixelIndex(x,y,color.rgbBlue);
		}
	}

	if (h) tmph.SetGrayPalette();
	if (s) tmps.SetGrayPalette();
	if (l) tmpl.SetGrayPalette();

	/* pseudo-color generator for hue channel (visual debug)
	if (h) for(long j=0; j<256; j++){
		BYTE i=(BYTE)j;
		RGBQUAD hsl={120,240,i,0};
		tmph.SetPaletteColor(i,HSLtoRGB(hsl));
	}*/

	if (h) h->Transfer(tmph);
	if (s) s->Transfer(tmps);
	if (l) l->Transfer(tmpl);

	return true;
}
////////////////////////////////////////////////////////////////////////////////
#define  HSLMAX   255	/* H,L, and S vary over 0-HSLMAX */
#define  RGBMAX   255   /* R,G, and B vary over 0-RGBMAX */
                        /* HSLMAX BEST IF DIVISIBLE BY 6 */
                        /* RGBMAX, HSLMAX must each fit in a BYTE. */
/* Hue is undefined if Saturation is 0 (grey-scale) */
/* This value determines where the Hue scrollbar is */
/* initially set for achromatic colors */
#define HSLUNDEFINED (HSLMAX*2/3)
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::RGBtoHSL(RGBQUAD lRGBColor)
{
	BYTE R,G,B;					/* input RGB values */
	BYTE H,L,S;					/* output HSL values */
	BYTE cMax,cMin;				/* max and min RGB values */
	WORD Rdelta,Gdelta,Bdelta;	/* intermediate value: % of spread from max*/

	R = lRGBColor.rgbRed;	/* get R, G, and B out of DWORD */
	G = lRGBColor.rgbGreen;
	B = lRGBColor.rgbBlue;

	cMax = max( max(R,G), B);	/* calculate lightness */
	cMin = min( min(R,G), B);
	L = (BYTE)((((cMax+cMin)*HSLMAX)+RGBMAX)/(2*RGBMAX));

	if (cMax==cMin){			/* r=g=b --> achromatic case */
		S = 0;					/* saturation */
		H = HSLUNDEFINED;		/* hue */
	} else {					/* chromatic case */
		if (L <= (HSLMAX/2))	/* saturation */
			S = (BYTE)((((cMax-cMin)*HSLMAX)+((cMax+cMin)/2))/(cMax+cMin));
		else
			S = (BYTE)((((cMax-cMin)*HSLMAX)+((2*RGBMAX-cMax-cMin)/2))/(2*RGBMAX-cMax-cMin));
		/* hue */
		Rdelta = (WORD)((((cMax-R)*(HSLMAX/6)) + ((cMax-cMin)/2) ) / (cMax-cMin));
		Gdelta = (WORD)((((cMax-G)*(HSLMAX/6)) + ((cMax-cMin)/2) ) / (cMax-cMin));
		Bdelta = (WORD)((((cMax-B)*(HSLMAX/6)) + ((cMax-cMin)/2) ) / (cMax-cMin));

		if (R == cMax)
			H = (BYTE)(Bdelta - Gdelta);
		else if (G == cMax)
			H = (BYTE)((HSLMAX/3) + Rdelta - Bdelta);
		else /* B == cMax */
			H = (BYTE)(((2*HSLMAX)/3) + Gdelta - Rdelta);

//		if (H < 0) H += HSLMAX;     //always false
		if (H > HSLMAX) H -= HSLMAX;
	}
	RGBQUAD hsl={L,S,H,0};
	return hsl;
}
////////////////////////////////////////////////////////////////////////////////
float CxImage::HueToRGB(float n1,float n2, float hue)
{
	//<F. Livraghi> fixed implementation for HSL2RGB routine
	float rValue;

	if (hue > 360)
		hue = hue - 360;
	else if (hue < 0)
		hue = hue + 360;

	if (hue < 60)
		rValue = n1 + (n2-n1)*hue/60.0f;
	else if (hue < 180)
		rValue = n2;
	else if (hue < 240)
		rValue = n1+(n2-n1)*(240-hue)/60;
	else
		rValue = n1;

	return rValue;
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::HSLtoRGB(COLORREF cHSLColor)
{
	return HSLtoRGB(RGBtoRGBQUAD(cHSLColor));
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::HSLtoRGB(RGBQUAD lHSLColor)
{ 
	//<F. Livraghi> fixed implementation for HSL2RGB routine
	float h,s,l;
	float m1,m2;
	BYTE r,g,b;

	h = (float)lHSLColor.rgbRed * 360.0f/255.0f;
	s = (float)lHSLColor.rgbGreen/255.0f;
	l = (float)lHSLColor.rgbBlue/255.0f;

	if (l <= 0.5)	m2 = l * (1+s);
	else			m2 = l + s - l*s;

	m1 = 2 * l - m2;

	if (s == 0) {
		r=g=b=(BYTE)(l*255.0f);
	} else {
		r = (BYTE)(HueToRGB(m1,m2,h+120) * 255.0f);
		g = (BYTE)(HueToRGB(m1,m2,h) * 255.0f);
		b = (BYTE)(HueToRGB(m1,m2,h-120) * 255.0f);
	}

	RGBQUAD rgb = {b,g,r,0};
	return rgb;
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::YUVtoRGB(RGBQUAD lYUVColor)
{
	int U,V,R,G,B;
	float Y = lYUVColor.rgbRed;
	U = lYUVColor.rgbGreen - 128;
	V = lYUVColor.rgbBlue - 128;

//	R = (int)(1.164 * Y + 2.018 * U);
//	G = (int)(1.164 * Y - 0.813 * V - 0.391 * U);
//	B = (int)(1.164 * Y + 1.596 * V);
	R = (int)( Y + 1.403f * V);
	G = (int)( Y - 0.344f * U - 0.714f * V);
	B = (int)( Y + 1.770f * U);

	R= min(255,max(0,R));
	G= min(255,max(0,G));
	B= min(255,max(0,B));
	RGBQUAD rgb={(BYTE)B,(BYTE)G,(BYTE)R,0};
	return rgb;
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::RGBtoYUV(RGBQUAD lRGBColor)
{
	int Y,U,V,R,G,B;
	R = lRGBColor.rgbRed;
	G = lRGBColor.rgbGreen;
	B = lRGBColor.rgbBlue;

//	Y = (int)( 0.257 * R + 0.504 * G + 0.098 * B);
//	U = (int)( 0.439 * R - 0.368 * G - 0.071 * B + 128);
//	V = (int)(-0.148 * R - 0.291 * G + 0.439 * B + 128);
	Y = (int)(0.299f * R + 0.587f * G + 0.114f * B);
	U = (int)((B-Y) * 0.565f + 128);
	V = (int)((R-Y) * 0.713f + 128);

	Y= min(255,max(0,Y));
	U= min(255,max(0,U));
	V= min(255,max(0,V));
	RGBQUAD yuv={(BYTE)V,(BYTE)U,(BYTE)Y,0};
	return yuv;
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::YIQtoRGB(RGBQUAD lYIQColor)
{
	int I,Q,R,G,B;
	float Y = lYIQColor.rgbRed;
	I = lYIQColor.rgbGreen - 128;
	Q = lYIQColor.rgbBlue - 128;

	R = (int)( Y + 0.956f * I + 0.621f * Q);
	G = (int)( Y - 0.273f * I - 0.647f * Q);
	B = (int)( Y - 1.104f * I + 1.701f * Q);

	R= min(255,max(0,R));
	G= min(255,max(0,G));
	B= min(255,max(0,B));
	RGBQUAD rgb={(BYTE)B,(BYTE)G,(BYTE)R,0};
	return rgb;
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::RGBtoYIQ(RGBQUAD lRGBColor)
{
	int Y,I,Q,R,G,B;
	R = lRGBColor.rgbRed;
	G = lRGBColor.rgbGreen;
	B = lRGBColor.rgbBlue;

	Y = (int)( 0.2992f * R + 0.5868f * G + 0.1140f * B);
	I = (int)( 0.5960f * R - 0.2742f * G - 0.3219f * B + 128);
	Q = (int)( 0.2109f * R - 0.5229f * G + 0.3120f * B + 128);

	Y= min(255,max(0,Y));
	I= min(255,max(0,I));
	Q= min(255,max(0,Q));
	RGBQUAD yiq={(BYTE)Q,(BYTE)I,(BYTE)Y,0};
	return yiq;
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::XYZtoRGB(RGBQUAD lXYZColor)
{
	int X,Y,Z,R,G,B;
	X = lXYZColor.rgbRed;
	Y = lXYZColor.rgbGreen;
	Z = lXYZColor.rgbBlue;
	double k=1.088751;

	R = (int)(  3.240479f * X - 1.537150f * Y - 0.498535f * Z * k);
	G = (int)( -0.969256f * X + 1.875992f * Y + 0.041556f * Z * k);
	B = (int)(  0.055648f * X - 0.204043f * Y + 1.057311f * Z * k);

	R= min(255,max(0,R));
	G= min(255,max(0,G));
	B= min(255,max(0,B));
	RGBQUAD rgb={(BYTE)B,(BYTE)G,(BYTE)R,0};
	return rgb;
}
////////////////////////////////////////////////////////////////////////////////
RGBQUAD CxImage::RGBtoXYZ(RGBQUAD lRGBColor)
{
	int X,Y,Z,R,G,B;
	R = lRGBColor.rgbRed;
	G = lRGBColor.rgbGreen;
	B = lRGBColor.rgbBlue;

	X = (int)( 0.412453f * R + 0.357580f * G + 0.180423f * B);
	Y = (int)( 0.212671f * R + 0.715160f * G + 0.072169f * B);
	Z = (int)((0.019334f * R + 0.119193f * G + 0.950227f * B)*0.918483657f);

	//X= min(255,max(0,X));
	//Y= min(255,max(0,Y));
	//Z= min(255,max(0,Z));
	RGBQUAD xyz={(BYTE)Z,(BYTE)Y,(BYTE)X,0};
	return xyz;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Generates a "rainbow" palette with saturated colors
 * \param correction: 1 generates a single hue spectrum. 0.75 is nice for scientific applications.
 */
void CxImage::HuePalette(float correction)
{
	if (head.biClrUsed==0) return;

	for(DWORD j=0; j<head.biClrUsed; j++){
		BYTE i=(BYTE)(j*correction*(255/(head.biClrUsed-1)));
		RGBQUAD hsl={120,240,i,0};
		SetPaletteColor((BYTE)j,HSLtoRGB(hsl));
	}
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Replaces the original hue and saturation values.
 * \param hue: hue
 * \param sat: saturation
 * \param blend: can be from 0 (no effect) to 1 (full effect)
 * \return true if everything is ok
 */
bool CxImage::Colorize(BYTE hue, BYTE sat, float blend)
{
	if (!pDib) return false;

	if (blend < 0.0f) blend = 0.0f;
	if (blend > 1.0f) blend = 1.0f;
	int a0 = (int)(256*blend);
	int a1 = 256 - a0;

	bool bFullBlend = false;
	if (blend > 0.999f)	bFullBlend = true;

	RGBQUAD color,hsl;
	if (head.biClrUsed==0){

		long xmin,xmax,ymin,ymax;
		if (pSelection){
			xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
			ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
		} else {
			xmin = ymin = 0;
			xmax = head.biWidth; ymax=head.biHeight;
		}

		for(long y=ymin; y<ymax; y++){
			for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
				if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
				{
					if (bFullBlend){
						color = RGBtoHSL(GetPixelColor(x,y));
						color.rgbRed=hue;
						color.rgbGreen=sat;
						SetPixelColor(x,y,HSLtoRGB(color));
					} else {
						color = GetPixelColor(x,y);
						hsl = RGBtoHSL(color);
						hsl.rgbRed=hue;
						hsl.rgbGreen=sat;
						hsl = HSLtoRGB(hsl);
						//BlendPixelColor(x,y,hsl,blend);
						//color.rgbRed = (BYTE)(hsl.rgbRed * blend + color.rgbRed * (1.0f - blend));
						//color.rgbBlue = (BYTE)(hsl.rgbBlue * blend + color.rgbBlue * (1.0f - blend));
						//color.rgbGreen = (BYTE)(hsl.rgbGreen * blend + color.rgbGreen * (1.0f - blend));
						color.rgbRed = (BYTE)((hsl.rgbRed * a0 + color.rgbRed * a1)>>8);
						color.rgbBlue = (BYTE)((hsl.rgbBlue * a0 + color.rgbBlue * a1)>>8);
						color.rgbGreen = (BYTE)((hsl.rgbGreen * a0 + color.rgbGreen * a1)>>8);
						SetPixelColor(x,y,color);
					}
				}
			}
		}
	} else {
		for(DWORD j=0; j<head.biClrUsed; j++){
			if (bFullBlend){
				color = RGBtoHSL(GetPaletteColor((BYTE)j));
				color.rgbRed=hue;
				color.rgbGreen=sat;
				SetPaletteColor((BYTE)j,HSLtoRGB(color));
			} else {
				color = GetPaletteColor((BYTE)j);
				hsl = RGBtoHSL(color);
				hsl.rgbRed=hue;
				hsl.rgbGreen=sat;
				hsl = HSLtoRGB(hsl);
				color.rgbRed = (BYTE)(hsl.rgbRed * blend + color.rgbRed * (1.0f - blend));
				color.rgbBlue = (BYTE)(hsl.rgbBlue * blend + color.rgbBlue * (1.0f - blend));
				color.rgbGreen = (BYTE)(hsl.rgbGreen * blend + color.rgbGreen * (1.0f - blend));
				SetPaletteColor((BYTE)j,color);
			}
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Changes the brightness and the contrast of the image. 
 * \param brightness: can be from -255 to 255, if brightness is negative, the image becomes dark.
 * \param contrast: can be from -100 to 100, the neutral value is 0.
 * \return true if everything is ok
 */
bool CxImage::Light(long brightness, long contrast)
{
	if (!pDib) return false;
	float c=(100 + contrast)/100.0f;
	brightness+=128;

	BYTE cTable[256]; //<nipper>
	for (int i=0;i<256;i++)	{
		cTable[i] = (BYTE)max(0,min(255,(int)((i-128)*c + brightness)));
	}

	return Lut(cTable);
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return mean lightness of the image. Useful with Threshold() and Light()
 */
float CxImage::Mean()
{
	if (!pDib) return 0;

	CxImage tmp(*this,true);
	if (!tmp.IsValid()) return false;

	tmp.GrayScale();
	float sum=0;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}
	if (xmin==xmax || ymin==ymax) return (float)0.0;

	BYTE *iSrc=tmp.info.pImage;
	iSrc += tmp.info.dwEffWidth*ymin; // necessary for selections <Admir Hodzic>

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/ymax); //<Anatoly Ivasyuk>
		for(long x=xmin; x<xmax; x++){
			sum+=iSrc[x];
		}
		iSrc+=tmp.info.dwEffWidth;
	}
	return sum/(xmax-xmin)/(ymax-ymin);
}
////////////////////////////////////////////////////////////////////////////////
/**
 * 2D linear filter
 * \param kernel: convolving matrix, in row format.
 * \param Ksize: size of the kernel.
 * \param Kfactor: normalization constant.
 * \param Koffset: bias.
 * \verbatim Example: the "soften" filter uses this kernel:
	1 1 1
	1 8 1
	1 1 1
 the function needs: kernel={1,1,1,1,8,1,1,1,1}; Ksize=3; Kfactor=16; Koffset=0; \endverbatim
 * \return true if everything is ok
 */
bool CxImage::Filter(long* kernel, long Ksize, long Kfactor, long Koffset)
{
	if (!pDib) return false;

	long k2 = Ksize/2;
	long kmax= Ksize-k2;
	long r,g,b,i;
	RGBQUAD c;

	CxImage tmp(*this,pSelection!=0,true,true);
	if (!tmp.IsValid()) return false;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	if ((head.biBitCount==8) && IsGrayScale())
	{
		unsigned char* cPtr;
		unsigned char* cPtr2;      
		int iCount;
		int iY, iY2, iY1;
		cPtr = info.pImage;
		cPtr2 = (unsigned char *)tmp.info.pImage;
		if (Kfactor==0) Kfactor = 1;
		for(long y=ymin; y<ymax; y++){
			info.nProgress = (long)(100*y/head.biHeight);
			if (info.nEscape) break;
			for(long x=xmin; x<xmax; x++){
				iY1 = y*info.dwEffWidth+x;
#if CXIMAGE_SUPPORT_SELECTION
				if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
				{
					if (y-k2 > 0 && (y+kmax-1) < head.biHeight && x-k2 > 0 && (x+kmax-1) < head.biWidth)
					{
						b=0;
						iCount = 0;
						iY2 = ((y-k2)*info.dwEffWidth);
						for(long j=-k2;j<kmax;j++)
						{
							iY = iY2+x;
							for(long k=-k2;k<kmax;k++)
							{
								i=kernel[iCount];
								b += cPtr[iY+k] * i;
								iCount++;
							}
							iY2 += info.dwEffWidth;
						}
						cPtr2[iY1] = (BYTE)min(255, max(0,(int)(b/Kfactor + Koffset)));
					}
					else
						cPtr2[iY1] = cPtr[iY1];
				}
			}
		}
	}
	else
	{
		for(long y=ymin; y<ymax; y++){
			info.nProgress = (long)(100*y/head.biHeight);
			if (info.nEscape) break;
			for(long x=xmin; x<xmax; x++){
	#if CXIMAGE_SUPPORT_SELECTION
				if (SelectionIsInside(x,y))
	#endif //CXIMAGE_SUPPORT_SELECTION
					{
					r=b=g=0;
					for(long j=-k2;j<kmax;j++){
						for(long k=-k2;k<kmax;k++){
							c=GetPixelColor(x+j,y+k);
							i=kernel[(j+k2)+Ksize*(k+k2)];
							r += c.rgbRed * i;
							g += c.rgbGreen * i;
							b += c.rgbBlue * i;
						}
					}
					if (Kfactor==0){
						c.rgbRed   = (BYTE)min(255, max(0,(int)(r + Koffset)));
						c.rgbGreen = (BYTE)min(255, max(0,(int)(g + Koffset)));
						c.rgbBlue  = (BYTE)min(255, max(0,(int)(b + Koffset)));
					} else {
						c.rgbRed   = (BYTE)min(255, max(0,(int)(r/Kfactor + Koffset)));
						c.rgbGreen = (BYTE)min(255, max(0,(int)(g/Kfactor + Koffset)));
						c.rgbBlue  = (BYTE)min(255, max(0,(int)(b/Kfactor + Koffset)));
					}
					tmp.SetPixelColor(x,y,c);
				}
			}
		}
	}
	Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Enhance the dark areas of the image
 * \param Ksize: size of the kernel.
 * \return true if everything is ok
 */
bool CxImage::Erode(long Ksize)
{
	if (!pDib) return false;

	long k2 = Ksize/2;
	long kmax= Ksize-k2;
	BYTE r,g,b;
	RGBQUAD c;

	CxImage tmp(*this,pSelection!=0,true,true);
	if (!tmp.IsValid()) return false;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/head.biHeight);
		if (info.nEscape) break;
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
			{
				r=b=g=255;
				for(long j=-k2;j<kmax;j++){
					for(long k=-k2;k<kmax;k++){
						c=GetPixelColor(x+j,y+k);
						if (c.rgbRed < r) r=c.rgbRed;
						if (c.rgbGreen < g) g=c.rgbGreen;
						if (c.rgbBlue < b) b=c.rgbBlue;
					}
				}
				c.rgbRed   = r;
				c.rgbGreen = g;
				c.rgbBlue  = b;
				tmp.SetPixelColor(x,y,c);
			}
		}
	}
	Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Enhance the light areas of the image
 * \param Ksize: size of the kernel.
 * \return true if everything is ok
 */
bool CxImage::Dilate(long Ksize)
{
	if (!pDib) return false;

	long k2 = Ksize/2;
	long kmax= Ksize-k2;
	BYTE r,g,b;
	RGBQUAD c;

	CxImage tmp(*this,pSelection!=0,true,true);
	if (!tmp.IsValid()) return false;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/head.biHeight);
		if (info.nEscape) break;
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
			{
				r=b=g=0;
				for(long j=-k2;j<kmax;j++){
					for(long k=-k2;k<kmax;k++){
						c=GetPixelColor(x+j,y+k);
						if (c.rgbRed > r) r=c.rgbRed;
						if (c.rgbGreen > g) g=c.rgbGreen;
						if (c.rgbBlue > b) b=c.rgbBlue;
					}
				}
				c.rgbRed   = r;
				c.rgbGreen = g;
				c.rgbBlue  = b;
				tmp.SetPixelColor(x,y,c);
			}
		}
	}
	Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Enhance the variations between adjacent pixels.
 * Similar results can be achieved using Filter(),
 * but the algorithms are different both in Edge() and in Contour().
 * \param Ksize: size of the kernel.
 * \return true if everything is ok
 */
bool CxImage::Edge(long Ksize)
{
	if (!pDib) return false;

	long k2 = Ksize/2;
	long kmax= Ksize-k2;
	BYTE r,g,b,rr,gg,bb;
	RGBQUAD c;

	CxImage tmp(*this,pSelection!=0,true,true);
	if (!tmp.IsValid()) return false;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/head.biHeight);
		if (info.nEscape) break;
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
			{
				r=b=g=0;
				rr=bb=gg=255;
				for(long j=-k2;j<kmax;j++){
					for(long k=-k2;k<kmax;k++){
						c=GetPixelColor(x+j,y+k);
						if (c.rgbRed > r) r=c.rgbRed;
						if (c.rgbGreen > g) g=c.rgbGreen;
						if (c.rgbBlue > b) b=c.rgbBlue;

						if (c.rgbRed < rr) rr=c.rgbRed;
						if (c.rgbGreen < gg) gg=c.rgbGreen;
						if (c.rgbBlue < bb) bb=c.rgbBlue;
					}
				}
				c.rgbRed   = 255-abs(r-rr);
				c.rgbGreen = 255-abs(g-gg);
				c.rgbBlue  = 255-abs(b-bb);
				tmp.SetPixelColor(x,y,c);
			}
		}
	}
	Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Blends two images
 * \param imgsrc2: image to be mixed with this
 * \param op: blending method; see ImageOpType
 * \param lXOffset, lYOffset: image displacement
 * \param bMixAlpha: if true and imgsrc2 has a valid alpha layer, it will be mixed in the destination image.
 * \return true if everything is ok
 *
 * thanks to Mwolski
 */
// 
void CxImage::Mix(CxImage & imgsrc2, ImageOpType op, long lXOffset, long lYOffset, bool bMixAlpha)
{
    long lWide = min(GetWidth(),imgsrc2.GetWidth()-lXOffset);
    long lHeight = min(GetHeight(),imgsrc2.GetHeight()-lYOffset);

	bool bEditAlpha = imgsrc2.AlphaIsValid() & bMixAlpha;

	if (bEditAlpha && AlphaIsValid()==false){
		AlphaCreate();
	}

    RGBQUAD rgbBackgrnd = GetTransColor();
    RGBQUAD rgb1, rgb2, rgbDest;

    for(long lY=0;lY<lHeight;lY++)
    {
		info.nProgress = (long)(100*lY/head.biHeight);
		if (info.nEscape) break;

        for(long lX=0;lX<lWide;lX++)
        {
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(lX,lY) && imgsrc2.SelectionIsInside(lX+lXOffset,lY+lYOffset))
#endif //CXIMAGE_SUPPORT_SELECTION
			{
				rgb1 = GetPixelColor(lX,lY);
				rgb2 = imgsrc2.GetPixelColor(lX+lXOffset,lY+lYOffset);
				switch(op)
				{
					case OpAdd:
						rgbDest.rgbBlue = (BYTE)max(0,min(255,rgb1.rgbBlue+rgb2.rgbBlue));
						rgbDest.rgbGreen = (BYTE)max(0,min(255,rgb1.rgbGreen+rgb2.rgbGreen));
						rgbDest.rgbRed = (BYTE)max(0,min(255,rgb1.rgbRed+rgb2.rgbRed));
						if (bEditAlpha) rgbDest.rgbReserved = (BYTE)max(0,min(255,rgb1.rgbReserved+rgb2.rgbReserved));
					break;
					case OpSub:
						rgbDest.rgbBlue = (BYTE)max(0,min(255,rgb1.rgbBlue-rgb2.rgbBlue));
						rgbDest.rgbGreen = (BYTE)max(0,min(255,rgb1.rgbGreen-rgb2.rgbGreen));
						rgbDest.rgbRed = (BYTE)max(0,min(255,rgb1.rgbRed-rgb2.rgbRed));
						if (bEditAlpha) rgbDest.rgbReserved = (BYTE)max(0,min(255,rgb1.rgbReserved-rgb2.rgbReserved));
					break;
					case OpAnd:
						rgbDest.rgbBlue = (BYTE)(rgb1.rgbBlue&rgb2.rgbBlue);
						rgbDest.rgbGreen = (BYTE)(rgb1.rgbGreen&rgb2.rgbGreen);
						rgbDest.rgbRed = (BYTE)(rgb1.rgbRed&rgb2.rgbRed);
						if (bEditAlpha) rgbDest.rgbReserved = (BYTE)(rgb1.rgbReserved&rgb2.rgbReserved);
					break;
					case OpXor:
						rgbDest.rgbBlue = (BYTE)(rgb1.rgbBlue^rgb2.rgbBlue);
						rgbDest.rgbGreen = (BYTE)(rgb1.rgbGreen^rgb2.rgbGreen);
						rgbDest.rgbRed = (BYTE)(rgb1.rgbRed^rgb2.rgbRed);
						if (bEditAlpha) rgbDest.rgbReserved = (BYTE)(rgb1.rgbReserved^rgb2.rgbReserved);
					break;
					case OpOr:
						rgbDest.rgbBlue = (BYTE)(rgb1.rgbBlue|rgb2.rgbBlue);
						rgbDest.rgbGreen = (BYTE)(rgb1.rgbGreen|rgb2.rgbGreen);
						rgbDest.rgbRed = (BYTE)(rgb1.rgbRed|rgb2.rgbRed);
						if (bEditAlpha) rgbDest.rgbReserved = (BYTE)(rgb1.rgbReserved|rgb2.rgbReserved);
					break;
					case OpMask:
						if(rgb2.rgbBlue==0 && rgb2.rgbGreen==0 && rgb2.rgbRed==0)
							rgbDest = rgbBackgrnd;
						else
							rgbDest = rgb1;
						break;
					case OpSrcCopy:
						if(memcmp(&rgb1,&rgbBackgrnd,sizeof(RGBQUAD))==0)
							rgbDest = rgb2;
						else // copy straight over
							rgbDest = rgb1;
						break;
					case OpDstCopy:
						if(memcmp(&rgb2,&rgbBackgrnd,sizeof(RGBQUAD))==0)
							rgbDest = rgb1;
						else // copy straight over
							rgbDest = rgb2;
						break;
					case OpScreen:
						{ 
							BYTE a,a1; 
							
							if (imgsrc2.IsTransparent(lX+lXOffset,lY+lYOffset)){
								a=0;
							} else if (imgsrc2.AlphaIsValid()){
								a=imgsrc2.AlphaGet(lX+lXOffset,lY+lYOffset);
								a =(BYTE)((a*(1+imgsrc2.info.nAlphaMax))>>8);
							} else {
								a=255;
							}

							if (a==0){ //transparent 
								rgbDest = rgb1; 
							} else if (a==255){ //opaque 
								rgbDest = rgb2; 
							} else { //blend 
								a1 = (BYTE)~a; 
								rgbDest.rgbBlue = (BYTE)((rgb1.rgbBlue*a1+rgb2.rgbBlue*a)>>8); 
								rgbDest.rgbGreen = (BYTE)((rgb1.rgbGreen*a1+rgb2.rgbGreen*a)>>8); 
								rgbDest.rgbRed = (BYTE)((rgb1.rgbRed*a1+rgb2.rgbRed*a)>>8);  
							}

							if (bEditAlpha) rgbDest.rgbReserved = (BYTE)(((1+rgb1.rgbReserved)*a)>>8);
						} 
						break; 
					case OpSrcBlend:
						if(memcmp(&rgb1,&rgbBackgrnd,sizeof(RGBQUAD))==0)
							rgbDest = rgb2;
						else
						{
							long lBDiff = abs(rgb1.rgbBlue - rgbBackgrnd.rgbBlue);
							long lGDiff = abs(rgb1.rgbGreen - rgbBackgrnd.rgbGreen);
							long lRDiff = abs(rgb1.rgbRed - rgbBackgrnd.rgbRed);

							double lAverage = (lBDiff+lGDiff+lRDiff)/3;
							double lThresh = 16;
							double dLarge = lAverage/lThresh;
							double dSmall = (lThresh-lAverage)/lThresh;
							double dSmallAmt = dSmall*((double)rgb2.rgbBlue);

							if( lAverage < lThresh+1){
								rgbDest.rgbBlue = (BYTE)max(0,min(255,(int)(dLarge*((double)rgb1.rgbBlue) +
												dSmallAmt)));
								rgbDest.rgbGreen = (BYTE)max(0,min(255,(int)(dLarge*((double)rgb1.rgbGreen) +
												dSmallAmt)));
								rgbDest.rgbRed = (BYTE)max(0,min(255,(int)(dLarge*((double)rgb1.rgbRed) +
												dSmallAmt)));
							}
							else
								rgbDest = rgb1;
						}
						break;
						default:
						return;
				}
				SetPixelColor(lX,lY,rgbDest,bEditAlpha);
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////
// thanks to Kenneth Ballard
void CxImage::MixFrom(CxImage & imagesrc2, long lXOffset, long lYOffset)
{
    RGBQUAD rgbBackgrnd = imagesrc2.GetTransColor();
    RGBQUAD rgb1;

    long width = imagesrc2.GetWidth();
    long height = imagesrc2.GetHeight();

    int x, y;

    for(x = 0; x < width; x++)
    {
        for(y = 0; y < height; y++)
        {
            rgb1 = imagesrc2.GetPixelColor(x, y);
            if(memcmp(&rgb1, &rgbBackgrnd, sizeof(RGBQUAD)) != 0)
                SetPixelColor(x + lXOffset, y + lYOffset, rgb1);
        }
    }
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Adjusts separately the red, green, and blue values in the image.
 * \param r, g, b: can be from -255 to +255.
 * \return true if everything is ok
 */
bool CxImage::ShiftRGB(long r, long g, long b)
{
	if (!pDib) return false;
	RGBQUAD color;
	if (head.biClrUsed==0){

		long xmin,xmax,ymin,ymax;
		if (pSelection){
			xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
			ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
		} else {
			xmin = ymin = 0;
			xmax = head.biWidth; ymax=head.biHeight;
		}

		for(long y=ymin; y<ymax; y++){
			for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
				if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
				{
					color = GetPixelColor(x,y);
					color.rgbRed = (BYTE)max(0,min(255,(int)(color.rgbRed + r)));
					color.rgbGreen = (BYTE)max(0,min(255,(int)(color.rgbGreen + g)));
					color.rgbBlue = (BYTE)max(0,min(255,(int)(color.rgbBlue + b)));
					SetPixelColor(x,y,color);
				}
			}
		}
	} else {
		for(DWORD j=0; j<head.biClrUsed; j++){
			color = GetPaletteColor((BYTE)j);
			color.rgbRed = (BYTE)max(0,min(255,(int)(color.rgbRed + r)));
			color.rgbGreen = (BYTE)max(0,min(255,(int)(color.rgbGreen + g)));
			color.rgbBlue = (BYTE)max(0,min(255,(int)(color.rgbBlue + b)));
			SetPaletteColor((BYTE)j,color);
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Adjusts the color balance of the image
 * \param gamma can be from 0.1 to 5.
 * \return true if everything is ok
 */
bool CxImage::Gamma(float gamma)
{
	if (!pDib) return false;

	double dinvgamma = 1/gamma;
	double dMax = pow(255.0, dinvgamma) / 255.0;

	BYTE cTable[256]; //<nipper>
	for (int i=0;i<256;i++)	{
		cTable[i] = (BYTE)max(0,min(255,(int)( pow((double)i, dinvgamma) / dMax)));
	}

	return Lut(cTable);
}
////////////////////////////////////////////////////////////////////////////////
#if CXIMAGE_SUPPORT_WINCE == 0
/**
 * Adjusts the intensity of each pixel to the median intensity of its surrounding pixels.
 * \param Ksize: size of the kernel.
 * \return true if everything is ok
 */
bool CxImage::Median(long Ksize)
{
	if (!pDib) return false;

	long k2 = Ksize/2;
	long kmax= Ksize-k2;
	long i,j,k;

	RGBQUAD* kernel = (RGBQUAD*)malloc(Ksize*Ksize*sizeof(RGBQUAD));

	CxImage tmp(*this,pSelection!=0,true,true);
	if (!tmp.IsValid()) return false;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/head.biHeight);
		if (info.nEscape) break;
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
				{
				for(j=-k2, i=0;j<kmax;j++)
					for(k=-k2;k<kmax;k++, i++)
						kernel[i]=GetPixelColor(x+j,y+k);

				qsort(kernel, i, sizeof(RGBQUAD), CompareColors);
				tmp.SetPixelColor(x,y,kernel[i/2]);
			}
		}
	}
	free(kernel);
	Transfer(tmp);
	return true;
}
#endif //CXIMAGE_SUPPORT_WINCE
////////////////////////////////////////////////////////////////////////////////
/**
 * Adds an uniform noise to the image
 * \param level: can be from 0 (no noise) to 255 (lot of noise).
 * \return true if everything is ok
 */
bool CxImage::Noise(long level)
{
	if (!pDib) return false;
	RGBQUAD color;

	long xmin,xmax,ymin,ymax,n;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/ymax); //<Anatoly Ivasyuk>
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
			{
				color = GetPixelColor(x,y);
				n=(long)((rand()/(float)RAND_MAX - 0.5)*level);
				color.rgbRed = (BYTE)max(0,min(255,(int)(color.rgbRed + n)));
				n=(long)((rand()/(float)RAND_MAX - 0.5)*level);
				color.rgbGreen = (BYTE)max(0,min(255,(int)(color.rgbGreen + n)));
				n=(long)((rand()/(float)RAND_MAX - 0.5)*level);
				color.rgbBlue = (BYTE)max(0,min(255,(int)(color.rgbBlue + n)));
				SetPixelColor(x,y,color);
			}
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Computes the bidimensional FFT or DFT of the image.
 * - The images are processed as grayscale
 * - If the dimensions of the image are a power of, 2 the FFT is performed automatically.
 * - If dstReal and/or dstImag are NULL, the resulting images replaces the original(s).
 * - Note: with 8 bits there is a HUGE loss in the dynamics. The function tries
 *   to keep an acceptable SNR, but 8bit = 48dB...
 *
 * \param srcReal, srcImag: source images: One can be NULL, but not both
 * \param dstReal, dstImag: destination images. Can be NULL.
 * \param direction: 1 = forward, -1 = inverse.
 * \param bForceFFT: if true, the images are resampled to make the dimensions a power of 2.
 * \param bMagnitude: if true, the real part returns the magnitude, the imaginary part returns the phase
 * \return true if everything is ok
 */
bool CxImage::FFT2(CxImage* srcReal, CxImage* srcImag, CxImage* dstReal, CxImage* dstImag,
				   long direction, bool bForceFFT, bool bMagnitude)
{
	//check if there is something to convert
	if (srcReal==NULL && srcImag==NULL) return false;

	long w,h;
	//get width and height
	if (srcReal) {
		w=srcReal->GetWidth();
		h=srcReal->GetHeight();
	} else {
		w=srcImag->GetWidth();
		h=srcImag->GetHeight();
	}

	bool bXpow2 = IsPowerof2(w);
	bool bYpow2 = IsPowerof2(h);
	//if bForceFFT, width AND height must be powers of 2
	if (bForceFFT && !(bXpow2 && bYpow2)) {
		long i;
		
		i=0;
		while((1<<i)<w) i++;
		w=1<<i;
		bXpow2=true;

		i=0;
		while((1<<i)<h) i++;
		h=1<<i;
		bYpow2=true;
	}

	// I/O images for FFT
	CxImage *tmpReal,*tmpImag;

	// select output
	tmpReal = (dstReal) ? dstReal : srcReal;
	tmpImag = (dstImag) ? dstImag : srcImag;

	// src!=dst -> copy the image
	if (srcReal && dstReal) tmpReal->Copy(*srcReal,true,false,false);
	if (srcImag && dstImag) tmpImag->Copy(*srcImag,true,false,false);

	// dst&&src are empty -> create new one, else turn to GrayScale
	if (srcReal==0 && dstReal==0){
		tmpReal = new CxImage(w,h,8);
		tmpReal->Clear(0);
		tmpReal->SetGrayPalette();
	} else {
		if (!tmpReal->IsGrayScale()) tmpReal->GrayScale();
	}
	if (srcImag==0 && dstImag==0){
		tmpImag = new CxImage(w,h,8);
		tmpImag->Clear(0);
		tmpImag->SetGrayPalette();
	} else {
		if (!tmpImag->IsGrayScale()) tmpImag->GrayScale();
	}

	if (!(tmpReal->IsValid() && tmpImag->IsValid())){
		if (srcReal==0 && dstReal==0) delete tmpReal;
		if (srcImag==0 && dstImag==0) delete tmpImag;
		return false;
	}

	//resample for FFT, if necessary 
	tmpReal->Resample(w,h,0);
	tmpImag->Resample(w,h,0);

	//ok, here we have 2 (w x h), grayscale images ready for a FFT

	double* real;
	double* imag;
	long j,k,m;

	_complex **grid;
	//double mean = tmpReal->Mean();
	/* Allocate memory for the grid */
	grid = (_complex **)malloc(w * sizeof(_complex));
	for (k=0;k<w;k++) {
		grid[k] = (_complex *)malloc(h * sizeof(_complex));
	}
	for (j=0;j<h;j++) {
		for (k=0;k<w;k++) {
			grid[k][j].x = tmpReal->GetPixelIndex(k,j)-128;
			grid[k][j].y = tmpImag->GetPixelIndex(k,j)-128;
		}
	}

	//DFT buffers
	double *real2,*imag2;
	real2 = (double*)malloc(max(w,h) * sizeof(double));
	imag2 = (double*)malloc(max(w,h) * sizeof(double));

	/* Transform the rows */
	real = (double *)malloc(w * sizeof(double));
	imag = (double *)malloc(w * sizeof(double));

	m=0;
	while((1<<m)<w) m++;

	for (j=0;j<h;j++) {
		for (k=0;k<w;k++) {
			real[k] = grid[k][j].x;
			imag[k] = grid[k][j].y;
		}

		if (bXpow2) FFT(direction,m,real,imag);
		else		DFT(direction,w,real,imag,real2,imag2);

		for (k=0;k<w;k++) {
			grid[k][j].x = real[k];
			grid[k][j].y = imag[k];
		}
	}
	free(real);
	free(imag);

	/* Transform the columns */
	real = (double *)malloc(h * sizeof(double));
	imag = (double *)malloc(h * sizeof(double));

	m=0;
	while((1<<m)<h) m++;

	for (k=0;k<w;k++) {
		for (j=0;j<h;j++) {
			real[j] = grid[k][j].x;
			imag[j] = grid[k][j].y;
		}

		if (bYpow2) FFT(direction,m,real,imag);
		else		DFT(direction,h,real,imag,real2,imag2);

		for (j=0;j<h;j++) {
			grid[k][j].x = real[j];
			grid[k][j].y = imag[j];
		}
	}
	free(real);
	free(imag);

	free(real2);
	free(imag2);

	/* converting from double to byte, there is a HUGE loss in the dynamics
	  "nn" tries to keep an acceptable SNR, but 8bit=48dB: don't ask more */
	double nn=pow((double)2,(double)log((double)max(w,h))/(double)log((double)2)-4);
	//reversed gain for reversed transform
	if (direction==-1) nn=1/nn;
	//bMagnitude : just to see it on the screen
	if (bMagnitude) nn*=4;

	for (j=0;j<h;j++) {
		for (k=0;k<w;k++) {
			if (bMagnitude){
				tmpReal->SetPixelIndex(k,j,(BYTE)max(0,min(255,(nn*(3+log(_cabs(grid[k][j])))))));
				if (grid[k][j].x==0){
					tmpImag->SetPixelIndex(k,j,(BYTE)max(0,min(255,(128+(atan(grid[k][j].y/0.0000000001)*nn)))));
				} else {
					tmpImag->SetPixelIndex(k,j,(BYTE)max(0,min(255,(128+(atan(grid[k][j].y/grid[k][j].x)*nn)))));
				}
			} else {
				tmpReal->SetPixelIndex(k,j,(BYTE)max(0,min(255,(128 + grid[k][j].x*nn))));
				tmpImag->SetPixelIndex(k,j,(BYTE)max(0,min(255,(128 + grid[k][j].y*nn))));
			}
		}
	}

	for (k=0;k<w;k++) free (grid[k]);
	free (grid);

	if (srcReal==0 && dstReal==0) delete tmpReal;
	if (srcImag==0 && dstImag==0) delete tmpImag;

	return true;
}
////////////////////////////////////////////////////////////////////////////////
bool CxImage::IsPowerof2(long x)
{
	long i=0;
	while ((1<<i)<x) i++;
	if (x==(1<<i)) return true;
	return false;
}
////////////////////////////////////////////////////////////////////////////////
/**
   This computes an in-place complex-to-complex FFT 
   x and y are the real and imaginary arrays of n=2^m points.
   o(n)=n*log2(n)
   dir =  1 gives forward transform
   dir = -1 gives reverse transform 
   Written by Paul Bourke, July 1998
   FFT algorithm by Cooley and Tukey, 1965 
*/
bool CxImage::FFT(int dir,int m,double *x,double *y)
{
	long nn,i,i1,j,k,i2,l,l1,l2;
	double c1,c2,tx,ty,t1,t2,u1,u2,z;

	/* Calculate the number of points */
	nn = 1<<m;

	/* Do the bit reversal */
	i2 = nn >> 1;
	j = 0;
	for (i=0;i<nn-1;i++) {
		if (i < j) {
			tx = x[i];
			ty = y[i];
			x[i] = x[j];
			y[i] = y[j];
			x[j] = tx;
			y[j] = ty;
		}
		k = i2;
		while (k <= j) {
			j -= k;
			k >>= 1;
		}
		j += k;
	}

	/* Compute the FFT */
	c1 = -1.0;
	c2 = 0.0;
	l2 = 1;
	for (l=0;l<m;l++) {
		l1 = l2;
		l2 <<= 1;
		u1 = 1.0;
		u2 = 0.0;
		for (j=0;j<l1;j++) {
			for (i=j;i<nn;i+=l2) {
				i1 = i + l1;
				t1 = u1 * x[i1] - u2 * y[i1];
				t2 = u1 * y[i1] + u2 * x[i1];
				x[i1] = x[i] - t1;
				y[i1] = y[i] - t2;
				x[i] += t1;
				y[i] += t2;
			}
			z =  u1 * c1 - u2 * c2;
			u2 = u1 * c2 + u2 * c1;
			u1 = z;
		}
		c2 = sqrt((1.0 - c1) / 2.0);
		if (dir == 1)
			c2 = -c2;
		c1 = sqrt((1.0 + c1) / 2.0);
	}

	/* Scaling for forward transform */
	if (dir == 1) {
		for (i=0;i<nn;i++) {
			x[i] /= (double)nn;
			y[i] /= (double)nn;
		}
	}

   return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
   Direct fourier transform o(n)=n^2
   Written by Paul Bourke, July 1998 
*/
bool CxImage::DFT(int dir,long m,double *x1,double *y1,double *x2,double *y2)
{
   long i,k;
   double arg;
   double cosarg,sinarg;
   
   for (i=0;i<m;i++) {
      x2[i] = 0;
      y2[i] = 0;
      arg = - dir * 2.0 * PI * i / (double)m;
      for (k=0;k<m;k++) {
         cosarg = cos(k * arg);
         sinarg = sin(k * arg);
         x2[i] += (x1[k] * cosarg - y1[k] * sinarg);
         y2[i] += (x1[k] * sinarg + y1[k] * cosarg);
      }
   }
   
   /* Copy the data back */
   if (dir == 1) {
      for (i=0;i<m;i++) {
         x1[i] = x2[i] / m;
         y1[i] = y2[i] / m;
      }
   } else {
      for (i=0;i<m;i++) {
         x1[i] = x2[i];
         y1[i] = y2[i];
      }
   }
   
   return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Combines different color components into a single image
 * \param r,g,b: color channels
 * \param a: alpha layer, can be NULL
 * \param colorspace: 0 = RGB, 1 = HSL, 2 = YUV, 3 = YIQ, 4 = XYZ 
 * \return true if everything is ok
 */
bool CxImage::Combine(CxImage* r,CxImage* g,CxImage* b,CxImage* a, long colorspace)
{
	if (r==0 || g==0 || b==0) return false;

	long w = r->GetWidth();
	long h = r->GetHeight();

	Create(w,h,24);

	g->Resample(w,h);
	b->Resample(w,h);

	if (a) {
		a->Resample(w,h);
#if CXIMAGE_SUPPORT_ALPHA
		AlphaCreate();
#endif //CXIMAGE_SUPPORT_ALPHA
	}

	RGBQUAD c;
	for (long y=0;y<h;y++){
		info.nProgress = (long)(100*y/h); //<Anatoly Ivasyuk>
		for (long x=0;x<w;x++){
			c.rgbRed=r->GetPixelIndex(x,y);
			c.rgbGreen=g->GetPixelIndex(x,y);
			c.rgbBlue=b->GetPixelIndex(x,y);
			switch (colorspace){
			case 1:
				SetPixelColor(x,y,HSLtoRGB(c));
				break;
			case 2:
				SetPixelColor(x,y,YUVtoRGB(c));
				break;
			case 3:
				SetPixelColor(x,y,YIQtoRGB(c));
				break;
			case 4:
				SetPixelColor(x,y,XYZtoRGB(c));
				break;
			default:
				SetPixelColor(x,y,c);
			}
#if CXIMAGE_SUPPORT_ALPHA
			if (a) AlphaSet(x,y,a->GetPixelIndex(x,y));
#endif //CXIMAGE_SUPPORT_ALPHA
		}
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Smart blurring to remove small defects, dithering or artifacts.
 * \param radius: normally between 0.01 and 0.5
 * \param niterations: should be trimmed with radius, to avoid blurring should be (radius*niterations)<1
 * \param colorspace: 0 = RGB, 1 = HSL, 2 = YUV, 3 = YIQ, 4 = XYZ 
 * \return true if everything is ok
 */
bool CxImage::Repair(float radius, long niterations, long colorspace)
{
	if (!IsValid()) return false;

	long w = GetWidth();
	long h = GetHeight();

	CxImage r,g,b;

	r.Create(w,h,8);
	g.Create(w,h,8);
	b.Create(w,h,8);

	switch (colorspace){
	case 1:
		SplitHSL(&r,&g,&b);
		break;
	case 2:
		SplitYUV(&r,&g,&b);
		break;
	case 3:
		SplitYIQ(&r,&g,&b);
		break;
	case 4:
		SplitXYZ(&r,&g,&b);
		break;
	default:
		SplitRGB(&r,&g,&b);
	}
	
	for (int i=0; i<niterations; i++){
		RepairChannel(&r,radius);
		RepairChannel(&g,radius);
		RepairChannel(&b,radius);
	}

	CxImage* a=NULL;
#if CXIMAGE_SUPPORT_ALPHA
	if (AlphaIsValid()){
		a = new CxImage();
		AlphaSplit(a);
	}
#endif

	Combine(&r,&g,&b,a,colorspace);

	delete a;

	return true;
}
////////////////////////////////////////////////////////////////////////////////
bool CxImage::RepairChannel(CxImage *ch, float radius)
{
	if (ch==NULL) return false;

	CxImage tmp(*ch);
	if (!tmp.IsValid()) return false;

	long w = ch->GetWidth()-1;
	long h = ch->GetHeight()-1;

	double correction,ix,iy,ixx,ixy,iyy,den,num;
	int x,y,xy0,xp1,xm1,yp1,ym1;
	for(x=1; x<w; x++){
		for(y=1; y<h; y++){

			xy0 = ch->GetPixelIndex(x,y);
			xm1 = ch->GetPixelIndex(x-1,y);
			xp1 = ch->GetPixelIndex(x+1,y);
			ym1 = ch->GetPixelIndex(x,y-1);
			yp1 = ch->GetPixelIndex(x,y+1);

			ix= (xp1-xm1)/2.0;
			iy= (yp1-ym1)/2.0;
			ixx= xp1 - 2.0 * xy0 + xm1;
			iyy= yp1 - 2.0 * xy0 + ym1;
			ixy=(ch->GetPixelIndex(x+1,y+1)+ch->GetPixelIndex(x-1,y-1)-
				 ch->GetPixelIndex(x-1,y+1)-ch->GetPixelIndex(x+1,y-1))/4.0;

			num= (1.0+iy*iy)*ixx - ix*iy*ixy + (1.0+ix*ix)*iyy;
			den= 1.0+ix*ix+iy*iy;
			correction = num/den;

			tmp.SetPixelIndex(x,y,(BYTE)min(255,max(0,(xy0 + radius * correction))));
		}
	}

	for (x=0;x<=w;x++){
		tmp.SetPixelIndex(x,0,ch->GetPixelIndex(x,0));
		tmp.SetPixelIndex(x,h,ch->GetPixelIndex(x,h));
	}
	for (y=0;y<=h;y++){
		tmp.SetPixelIndex(0,y,ch->GetPixelIndex(0,y));
		tmp.SetPixelIndex(w,y,ch->GetPixelIndex(w,y));
	}
	ch->Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Enhance the variations between adjacent pixels.
 * Similar results can be achieved using Filter(),
 * but the algorithms are different both in Edge() and in Contour().
 * \return true if everything is ok
 */
bool CxImage::Contour()
{
	if (!pDib) return false;

	long Ksize = 3;
	long k2 = Ksize/2;
	long kmax= Ksize-k2;
	long i,j,k;
	BYTE maxr,maxg,maxb;
	RGBQUAD pix1,pix2;

	CxImage tmp(*this,pSelection!=0,true,true);
	if (!tmp.IsValid()) return false;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/head.biHeight);
		if (info.nEscape) break;
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
				{
				pix1 = GetPixelColor(x,y);
				maxr=maxg=maxb=0;
				for(j=-k2, i=0;j<kmax;j++){
					for(k=-k2;k<kmax;k++, i++){
						pix2=GetPixelColor(x+j,y+k);
						if ((pix2.rgbBlue-pix1.rgbBlue)>maxb) maxb = pix2.rgbBlue;
						if ((pix2.rgbGreen-pix1.rgbGreen)>maxg) maxg = pix2.rgbGreen;
						if ((pix2.rgbRed-pix1.rgbRed)>maxr) maxr = pix2.rgbRed;
					}
				}
				pix1.rgbBlue=(BYTE)(255-maxb);
				pix1.rgbGreen=(BYTE)(255-maxg);
				pix1.rgbRed=(BYTE)(255-maxr);
				tmp.SetPixelColor(x,y,pix1);
			}
		}
	}
	Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Adds a random offset to each pixel in the image
 * \param radius: maximum pixel displacement
 * \return true if everything is ok
 */
bool CxImage::Jitter(long radius)
{
	if (!pDib) return false;

	long nx,ny;

	CxImage tmp(*this,pSelection!=0,true,true);
	if (!tmp.IsValid()) return false;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		info.nProgress = (long)(100*y/head.biHeight);
		if (info.nEscape) break;
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
			{
				nx=x+(long)((rand()/(float)RAND_MAX - 0.5)*(radius*2));
				ny=y+(long)((rand()/(float)RAND_MAX - 0.5)*(radius*2));
				if (!IsInside(nx,ny)) {
					nx=x;
					ny=y;
				}
				if (head.biClrUsed==0){
					tmp.SetPixelColor(x,y,GetPixelColor(nx,ny));
				} else {
					tmp.SetPixelIndex(x,y,GetPixelIndex(nx,ny));
				}
#if CXIMAGE_SUPPORT_ALPHA
				tmp.AlphaSet(x,y,AlphaGet(nx,ny));
#endif //CXIMAGE_SUPPORT_ALPHA
			}
		}
	}
	Transfer(tmp);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
/** 
 * generates a 1-D convolution matrix to be used for each pass of 
 * a two-pass gaussian blur.  Returns the length of the matrix.
 * \author [nipper]
 */
int CxImage::gen_convolve_matrix (float radius, float **cmatrix_p)
{
	int matrix_length;
	int matrix_midpoint;
	float* cmatrix;
	int i,j;
	float std_dev;
	float sum;
	
	/* we want to generate a matrix that goes out a certain radius
	* from the center, so we have to go out ceil(rad-0.5) pixels,
	* inlcuding the center pixel.  Of course, that's only in one direction,
	* so we have to go the same amount in the other direction, but not count
	* the center pixel again.  So we double the previous result and subtract
	* one.
	* The radius parameter that is passed to this function is used as
	* the standard deviation, and the radius of effect is the
	* standard deviation * 2.  It's a little confusing.
	*/
	radius = (float)fabs(radius) + 1.0f;
	
	std_dev = radius;
	radius = std_dev * 2;
	
	/* go out 'radius' in each direction */
	matrix_length = int (2 * ceil(radius-0.5) + 1);
	if (matrix_length <= 0) matrix_length = 1;
	matrix_midpoint = matrix_length/2 + 1;
	*cmatrix_p = new float[matrix_length];
	cmatrix = *cmatrix_p;
	
	/*  Now we fill the matrix by doing a numeric integration approximation
	* from -2*std_dev to 2*std_dev, sampling 50 points per pixel.
	* We do the bottom half, mirror it to the top half, then compute the
	* center point.  Otherwise asymmetric quantization errors will occur.
	*  The formula to integrate is e^-(x^2/2s^2).
	*/
	
	/* first we do the top (right) half of matrix */
	for (i = matrix_length/2 + 1; i < matrix_length; i++)
    {
		float base_x = i - (float)floor((float)(matrix_length/2)) - 0.5f;
		sum = 0;
		for (j = 1; j <= 50; j++)
		{
			if ( base_x+0.02*j <= radius ) 
				sum += (float)exp (-(base_x+0.02*j)*(base_x+0.02*j) / 
				(2*std_dev*std_dev));
		}
		cmatrix[i] = sum/50;
    }
	
	/* mirror the thing to the bottom half */
	for (i=0; i<=matrix_length/2; i++) {
		cmatrix[i] = cmatrix[matrix_length-1-i];
	}
	
	/* find center val -- calculate an odd number of quanta to make it symmetric,
	* even if the center point is weighted slightly higher than others. */
	sum = 0;
	for (j=0; j<=50; j++)
    {
		sum += (float)exp (-(0.5+0.02*j)*(0.5+0.02*j) /
			(2*std_dev*std_dev));
    }
	cmatrix[matrix_length/2] = sum/51;
	
	/* normalize the distribution by scaling the total sum to one */
	sum=0;
	for (i=0; i<matrix_length; i++) sum += cmatrix[i];
	for (i=0; i<matrix_length; i++) cmatrix[i] = cmatrix[i] / sum;
	
	return matrix_length;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * generates a lookup table for every possible product of 0-255 and
 * each value in the convolution matrix.  The returned array is
 * indexed first by matrix position, then by input multiplicand (?)
 * value.
 * \author [nipper]
 */
float* CxImage::gen_lookup_table (float *cmatrix, int cmatrix_length)
{
	float* lookup_table = new float[cmatrix_length * 256];
	float* lookup_table_p = lookup_table;
	float* cmatrix_p      = cmatrix;
	
	for (int i=0; i<cmatrix_length; i++)
    {
		for (int j=0; j<256; j++)
		{
			*(lookup_table_p++) = *cmatrix_p * (float)j;
		}
		cmatrix_p++;
    }
	
	return lookup_table;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * this function is written as if it is blurring a column at a time,
 * even though it can operate on rows, too.  There is no difference
 * in the processing of the lines, at least to the blur_line function.
 * \author [nipper]
 */
void CxImage::blur_line (float *ctable, float *cmatrix, int cmatrix_length, BYTE* cur_col, BYTE* dest_col, int y, long bytes)
{
	float scale;
	float sum;
	int i=0, j=0;
	int row;
	int cmatrix_middle = cmatrix_length/2;
	
	float *cmatrix_p;
	BYTE  *cur_col_p;
	BYTE  *cur_col_p1;
	BYTE  *dest_col_p;
	float *ctable_p;
	
	/* this first block is the same as the non-optimized version --
	* it is only used for very small pictures, so speed isn't a
	* big concern.
	*/
	if (cmatrix_length > y)
    {
		for (row = 0; row < y ; row++)
		{
			scale=0;
			/* find the scale factor */
			for (j = 0; j < y ; j++)
			{
				/* if the index is in bounds, add it to the scale counter */
				if ((j + cmatrix_length/2 - row >= 0) &&
					(j + cmatrix_length/2 - row < cmatrix_length))
					scale += cmatrix[j + cmatrix_length/2 - row];
			}
			for (i = 0; i<bytes; i++)
			{
				sum = 0;
				for (j = 0; j < y; j++)
				{
					if ((j >= row - cmatrix_length/2) &&
						(j <= row + cmatrix_length/2))
						sum += cur_col[j*bytes + i] * cmatrix[j];
				}
				dest_col[row*bytes + i] = (BYTE)(0.5f + sum / scale);
			}
		}
    }
	else
    {
		/* for the edge condition, we only use available info and scale to one */
		for (row = 0; row < cmatrix_middle; row++)
		{
			/* find scale factor */
			scale=0;
			for (j = cmatrix_middle - row; j<cmatrix_length; j++)
				scale += cmatrix[j];
			for (i = 0; i<bytes; i++)
			{
				sum = 0;
				for (j = cmatrix_middle - row; j<cmatrix_length; j++)
				{
					sum += cur_col[(row + j-cmatrix_middle)*bytes + i] * cmatrix[j];
				}
				dest_col[row*bytes + i] = (BYTE)(0.5f + sum / scale);
			}
		}
		/* go through each pixel in each col */
		dest_col_p = dest_col + row*bytes;
		for (; row < y-cmatrix_middle; row++)
		{
			cur_col_p = (row - cmatrix_middle) * bytes + cur_col;
			for (i = 0; i<bytes; i++)
			{
				sum = 0;
				cmatrix_p = cmatrix;
				cur_col_p1 = cur_col_p;
				ctable_p = ctable;
				for (j = cmatrix_length; j>0; j--)
				{
					sum += *(ctable_p + *cur_col_p1);
					cur_col_p1 += bytes;
					ctable_p += 256;
				}
				cur_col_p++;
				*(dest_col_p++) = (BYTE)(0.5f + sum);
			}
		}
		
		/* for the edge condition , we only use available info, and scale to one */
		for (; row < y; row++)
		{
			/* find scale factor */
			scale=0;
			for (j = 0; j< y-row + cmatrix_middle; j++)
				scale += cmatrix[j];
			for (i = 0; i<bytes; i++)
			{
				sum = 0;
				for (j = 0; j<y-row + cmatrix_middle; j++)
				{
					sum += cur_col[(row + j-cmatrix_middle)*bytes + i] * cmatrix[j];
				}
				dest_col[row*bytes + i] = (BYTE) (0.5f + sum / scale);
			}
		}
    }
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \author [nipper]
 */
bool CxImage::UnsharpMask(float radius /*= 5.0*/, float amount /*= 0.5*/, int threshold /*= 0*/)
{
	if (!pDib) return false;

	if (!(head.biBitCount == 24 || IsGrayScale()))
		return false;

	CxImage tmp(*this);
	if (!tmp.IsValid()) return false;

	CImageIterator itSrc(this);
	CImageIterator itDst(&tmp);

	// generate convolution matrix and make sure it's smaller than each dimension
	float *cmatrix = NULL;
	int cmatrix_length = gen_convolve_matrix(radius, &cmatrix);
	// generate lookup table
	float *ctable = gen_lookup_table(cmatrix, cmatrix_length);

	double dbScaler = 33.3/head.biHeight;
	int y;

	// blur the rows
    for (y=0;y<head.biHeight;y++)
	{
		if (info.nEscape) break;
		info.nProgress = (long)(y*dbScaler);

		blur_line(ctable, cmatrix, cmatrix_length, itSrc.GetRow(y), itDst.GetRow(y), head.biWidth, 3);
	}

	// blur the cols
    BYTE* cur_col = new BYTE[head.biHeight*3];
    BYTE* dest_col = new BYTE[head.biHeight*3];

	dbScaler = 33.3/head.biWidth;

	for (int x=0;x<head.biWidth;x++)
	{
		if (info.nEscape) break;
		info.nProgress = (long)(33.3+x*dbScaler);

		itSrc.GetCol(cur_col, x);
		itDst.GetCol(dest_col, x);
		blur_line(ctable, cmatrix, cmatrix_length, cur_col, dest_col, head.biHeight, 3);
		itSrc.SetCol(cur_col, x);
		itDst.SetCol(dest_col, x);
	}

	delete [] cur_col;
	delete [] dest_col;

	delete [] cmatrix;
	delete [] ctable;

	// these are used for the merging step 
	int diff;
	int value;

	dbScaler = 33.3/head.biHeight;
	
	// merge the source and destination (which currently contains
	// the blurred version) images
    for (y=0;y<head.biHeight;y++)
	{
		if (info.nEscape) break;
		info.nProgress = (long)(66.6+y*dbScaler);

		value = 0;
		// get source row
		BYTE* cur_row = itSrc.GetRow(y);
		// get dest row
		BYTE* dest_row = itDst.GetRow(y);
		// combine the two
		for (int u = 0; u < head.biWidth; u++)
		{
			for (int v = 0; v < 3; v++)
			{
				diff = (cur_row[u*3+v] - dest_row[u*3+v]);
				// do tresholding
				if (abs (2 * diff) < threshold)
					diff = 0;
	
				value = int(cur_row[u*3+v] + amount * diff);

				if (value < 0) dest_row[u*3+v] =0;
				else if (value > 255) dest_row[u*3+v] = 255;
				else  dest_row[u*3+v] = value;
			}
		}
	}

	Transfer(tmp);

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Apply a look up table to the image. 
 * \param pLut: BYTE[256] look up table
 * \return true if everything is ok
 */
bool CxImage::Lut(BYTE* pLut)
{
	if (!pDib || !pLut) return false;
	RGBQUAD color;

	double dbScaler;
	if (head.biClrUsed==0){

		long xmin,xmax,ymin,ymax;
		if (pSelection){
			xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
			ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
		} else {
			// faster loop for full image
			BYTE *iSrc=info.pImage;
			for(unsigned long i=0; i < head.biSizeImage ; i++){
				*iSrc++ = pLut[*iSrc];
			}
			return true;
		}

		dbScaler = 100.0/ymax;

		for(long y=ymin; y<ymax; y++){
			info.nProgress = (long)(y*dbScaler); //<Anatoly Ivasyuk>
			for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
				if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
				{
					color = GetPixelColor(x,y);
					color.rgbRed = pLut[color.rgbRed];
					color.rgbGreen = pLut[color.rgbGreen];
					color.rgbBlue = pLut[color.rgbBlue];
					SetPixelColor(x,y,color);
				}
			}
		}
#if CXIMAGE_SUPPORT_SELECTION
	} else if (pSelection && (head.biBitCount==8) && IsGrayScale()){
		long xmin,xmax,ymin,ymax;
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;

		dbScaler = 100.0/ymax;
		for(long y=ymin; y<ymax; y++){
			info.nProgress = (long)(y*dbScaler);
			for(long x=xmin; x<xmax; x++){
				if (SelectionIsInside(x,y))
				{
					SetPixelIndex(x,y,pLut[GetPixelIndex(x,y)]);
				}
			}
		}
#endif //CXIMAGE_SUPPORT_SELECTION
	} else {
		for(DWORD j=0; j<head.biClrUsed; j++){
			color = GetPaletteColor((BYTE)j);
			color.rgbRed = pLut[color.rgbRed];
			color.rgbGreen = pLut[color.rgbGreen];
			color.rgbBlue = pLut[color.rgbBlue];
			SetPaletteColor((BYTE)j,color);
		}
	}
	return true;

}
////////////////////////////////////////////////////////////////////////////////
/**
 * Apply an indipendent look up table for each channel
 * \param pLutR, pLutG, pLutB, pLutA: BYTE[256] look up tables
 * \return true if everything is ok
 */
bool CxImage::Lut(BYTE* pLutR, BYTE* pLutG, BYTE* pLutB, BYTE* pLutA)
{
	if (!pDib || !pLutR || !pLutG || !pLutB) return false;
	RGBQUAD color;

	double dbScaler;
	if (head.biClrUsed==0){

		long xmin,xmax,ymin,ymax;
		if (pSelection){
			xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
			ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
		} else {
			xmin = ymin = 0;
			xmax = head.biWidth; ymax=head.biHeight;
		}

		dbScaler = 100.0/ymax;

		for(long y=ymin; y<ymax; y++){
			info.nProgress = (long)(y*dbScaler);
			for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
				if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
				{
					color = GetPixelColor(x,y);
					color.rgbRed =   pLutR[color.rgbRed];
					color.rgbGreen = pLutG[color.rgbGreen];
					color.rgbBlue =  pLutB[color.rgbBlue];
					if (pLutA) color.rgbReserved=pLutA[color.rgbReserved];
					SetPixelColor(x,y,color,true);
				}
			}
		}
	} else {
		for(DWORD j=0; j<head.biClrUsed; j++){
			color = GetPaletteColor((BYTE)j);
			color.rgbRed =   pLutR[color.rgbRed];
			color.rgbGreen = pLutG[color.rgbGreen];
			color.rgbBlue =  pLutB[color.rgbBlue];
			SetPaletteColor((BYTE)j,color);
		}
	}

	return true;

}
////////////////////////////////////////////////////////////////////////////////
/**
 * Use the RedEyeRemove function to remove the red-eye effect that frequently
 * occurs in photographs of humans and animals. You must select the region 
 * where the function will filter the red channel.
 * \return true if everything is ok
 */
bool CxImage::RedEyeRemove()
{
	if (!pDib) return false;
	RGBQUAD color;

	long xmin,xmax,ymin,ymax;
	if (pSelection){
		xmin = info.rSelectionBox.left; xmax = info.rSelectionBox.right;
		ymin = info.rSelectionBox.bottom; ymax = info.rSelectionBox.top;
	} else {
		xmin = ymin = 0;
		xmax = head.biWidth; ymax=head.biHeight;
	}

	for(long y=ymin; y<ymax; y++){
		for(long x=xmin; x<xmax; x++){
#if CXIMAGE_SUPPORT_SELECTION
			if (SelectionIsInside(x,y))
#endif //CXIMAGE_SUPPORT_SELECTION
			{
				color = GetPixelColor(x,y);
				color.rgbRed = min(color.rgbGreen,color.rgbBlue);
				SetPixelColor(x,y,color);
			}
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
/*bool CxImage::FloodFill(int x, int y, RGBQUAD FillColor)
{
	//<JDL>
	if (!pDib) return false;
    FloodFill2(x,y,GetPixelColor(x,y),FillColor);
	return true;
}
////////////////////////////////////////////////////////////////////////////////
void CxImage::FloodFill2(int x, int y, RGBQUAD old_color, RGBQUAD new_color)
{
	// Fill in the actual pixels. 
	// Function steps recursively until it finds borders (color that is not old_color)
	if (!IsInside(x,y)) return;

	RGBQUAD r = GetPixelColor(x,y);
	COLORREF cr = RGB(r.rgbRed,r.rgbGreen,r.rgbBlue);

	if(cr == RGB(old_color.rgbRed,old_color.rgbGreen,old_color.rgbBlue)
		&& cr != RGB(new_color.rgbRed,new_color.rgbGreen,new_color.rgbBlue) ) {

		// the above if statement, after && is there to prevent
		// stack overflows.  The program will continue to find 
		// colors if you flood-fill an entire region (entire picture)

		SetPixelColor(x,y,new_color);

		FloodFill2((x+1),y,old_color,new_color);
		FloodFill2((x-1),y,old_color,new_color);
		FloodFill2(x,(y+1),old_color,new_color);
		FloodFill2(x,(y-1),old_color,new_color);
	}
}*/
///////////////////////////////////////////////////////////////////////////////
#endif //CXIMAGE_SUPPORT_DSP
