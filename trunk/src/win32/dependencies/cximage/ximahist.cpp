// xImaHist.cpp : histogram functions
/* 28/01/2004 v1.00 - www.xdp.it
 * CxImage version 5.99c 17/Oct/2004
 */

#include "ximage.h"

#if CXIMAGE_SUPPORT_DSP

////////////////////////////////////////////////////////////////////////////////
long CxImage::Histogram(long* red, long* green, long* blue, long* gray, long colorspace)
{
	if (!pDib) return 0;
	RGBQUAD color;

	if (red) memset(red,0,256*sizeof(long));
	if (green) memset(green,0,256*sizeof(long));
	if (blue) memset(blue,0,256*sizeof(long));
	if (gray) memset(gray,0,256*sizeof(long));

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
				switch (colorspace){
				case 1:
					color = HSLtoRGB(GetPixelColor(x,y));
					break;
				case 2:
					color = YUVtoRGB(GetPixelColor(x,y));
					break;
				case 3:
					color = YIQtoRGB(GetPixelColor(x,y));
					break;
				case 4:
					color = XYZtoRGB(GetPixelColor(x,y));
					break;
				default:
					color = GetPixelColor(x,y);
				}

				if (red) red[color.rgbRed]++;
				if (green) green[color.rgbGreen]++;
				if (blue) blue[color.rgbBlue]++;
				if (gray) gray[(BYTE)RGB2GRAY(color.rgbRed,color.rgbGreen,color.rgbBlue)]++;
			}
		}
	}

	long n=0;
	for (int i=0; i<256; i++){
		if (red && red[i]>n) n=red[i];
		if (green && green[i]>n) n=green[i];
		if (blue && blue[i]>n) n=blue[i];
		if (gray && gray[i]>n) n=gray[i];
	}

	return n;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * HistogramStretch
 * \param method: 0 = luminance (default), 1 = linked channels , 2 = independent channels.
 * \return true if everything is ok
 * \author [dave] and [nipper]
 */
bool CxImage::HistogramStretch(long method)
{
  if (!pDib) return false;

  if ((head.biBitCount==8) && IsGrayScale()){
	// get min/max info
	BYTE minc = 255, maxc = 0;
	BYTE gray;
	long y;

	double dbScaler = 50.0/head.biHeight;

	for (y=0; y<head.biHeight; y++)
	{
		info.nProgress = (long)(y*dbScaler);
		if (info.nEscape) break;
		for (long x=0; x<head.biWidth; x++)	{
			gray = GetPixelIndex(x, y);
			if (gray < minc)   minc = gray;
			if (gray > maxc)   maxc = gray; 
		}
	}

	if (minc == 0 && maxc == 255) return true;
	
	// calculate LUT
	BYTE lut[256];
	BYTE range = maxc - minc;
	if (range != 0){
		for (long x = minc; x <= maxc; x++){
			lut[x] = (BYTE)(255 * (x - minc) / range);
		}
	} else lut[minc] = minc;

	for (y=0; y<head.biHeight; y++)	{
		if (info.nEscape) break;
		info.nProgress = (long)(50.0+y*dbScaler);
		for (long x=0; x<head.biWidth; x++)
		{
			SetPixelIndex(x, y, lut[GetPixelIndex(x, y)]);
		}
	}
  } else {
	switch(method){
	case 1:
	  { // <nipper>
		// get min/max info
		BYTE minc = 255, maxc = 0;
		RGBQUAD color;
		long y;

		for (y=0; y<head.biHeight; y++)
		{
			if (info.nEscape) break;

			for (long x=0; x<head.biWidth; x++)
			{
				color = GetPixelColor(x, y);

				if (color.rgbRed < minc)   minc = color.rgbRed;
				if (color.rgbBlue < minc)  minc = color.rgbBlue;
				if (color.rgbGreen < minc) minc = color.rgbGreen;

				if (color.rgbRed > maxc)   maxc = color.rgbRed; 
				if (color.rgbBlue > maxc)  maxc = color.rgbBlue; 
				if (color.rgbGreen > maxc) maxc = color.rgbGreen; 
			}
		}

		if (minc == 0 && maxc == 255)
			return true;
		
		// calculate LUT
		BYTE lut[256];
		BYTE range = maxc - minc;

		if (range != 0){
			for (long x = minc; x <= maxc; x++){
				lut[x] = (BYTE)(255 * (x - minc) / range);
			}
		} else lut[minc] = minc;

		// normalize image
		double dbScaler = 100.0/head.biHeight;

		for (y=0; y<head.biHeight; y++)	{
			if (info.nEscape) break;
			info.nProgress = (long)(y*dbScaler);

			for (long x=0; x<head.biWidth; x++)
			{
				color = GetPixelColor(x, y);

				color.rgbRed = lut[color.rgbRed];
				color.rgbBlue = lut[color.rgbBlue];
				color.rgbGreen = lut[color.rgbGreen];

				SetPixelColor(x, y, color);
			}
		}
	  }
		break;
	case 2:
	  { // <nipper>
		// get min/max info
		BYTE minR = 255, maxR = 0;
		BYTE minG = 255, maxG = 0;
		BYTE minB = 255, maxB = 0;
		RGBQUAD color;
		long y;

		for (y=0; y<head.biHeight; y++)
		{
			if (info.nEscape) break;

			for (long x=0; x<head.biWidth; x++)
			{
				color = GetPixelColor(x, y);

				if (color.rgbRed < minR)   minR = color.rgbRed;
				if (color.rgbBlue < minB)  minB = color.rgbBlue;
				if (color.rgbGreen < minG) minG = color.rgbGreen;

				if (color.rgbRed > maxR)   maxR = color.rgbRed; 
				if (color.rgbBlue > maxB)  maxB = color.rgbBlue; 
				if (color.rgbGreen > maxG) maxG = color.rgbGreen; 
			}
		}

		if (minR == 0 && maxR == 255 && minG == 0 && maxG == 255 && minB == 0 && maxB == 255)
			return true;

		// calculate LUT
		BYTE lutR[256];
		BYTE range = maxR - minR;
		if (range != 0)	{
			for (long x = minR; x <= maxR; x++){
				lutR[x] = (BYTE)(255 * (x - minR) / range);
			}
		} else lutR[minR] = minR;

		BYTE lutG[256];
		range = maxG - minG;
		if (range != 0)	{
			for (long x = minG; x <= maxG; x++){
				lutG[x] = (BYTE)(255 * (x - minG) / range);
			}
		} else lutG[minG] = minG;
			
		BYTE lutB[256];
		range = maxB - minB;
		if (range != 0)	{
			for (long x = minB; x <= maxB; x++){
				lutB[x] = (BYTE)(255 * (x - minB) / range);
			}
		} else lutB[minB] = minB;

		// normalize image
		double dbScaler = 100.0/head.biHeight;

		for (y=0; y<head.biHeight; y++)
		{
			info.nProgress = (long)(y*dbScaler);
			if (info.nEscape) break;

			for (long x=0; x<head.biWidth; x++)
			{
				color = GetPixelColor(x, y);

				color.rgbRed = lutR[color.rgbRed];
				color.rgbBlue = lutB[color.rgbBlue];
				color.rgbGreen = lutG[color.rgbGreen];

				SetPixelColor(x, y, color);
			}
		}
	  }
		break;
	default:
	  { // <dave>
		// S = ( R - C ) ( B - A / D - C )
		double alimit = 0.0;
		double blimit = 255.0;
		double lowerc = 255.0;
		double upperd = 0.0;
		double tmpGray;

		RGBQUAD color;
		RGBQUAD	yuvClr;
		double  stretcheds;

		if ( head.biClrUsed == 0 ){
			long x, y, xmin, xmax, ymin, ymax;
			xmin = ymin = 0;
			xmax = head.biWidth; 
			ymax = head.biHeight;

			for( y = ymin; y < ymax; y++ ){
				info.nProgress = (long)(50*y/ymax);
				if (info.nEscape) break;
				for( x = xmin; x < xmax; x++ ){
					color = GetPixelColor( x, y );
					tmpGray = RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
					if ( tmpGray < lowerc )	lowerc = tmpGray;
					if ( tmpGray > upperd )	upperd = tmpGray;
				}
			}
			if (upperd==lowerc) return false;
			
			for( y = ymin; y < ymax; y++ ){
				info.nProgress = (long)(50+50*y/ymax);
				if (info.nEscape) break;
				for( x = xmin; x < xmax; x++ ){

					color = GetPixelColor( x, y );
					yuvClr = RGBtoYUV(color);

					// Stretch Luminance
					tmpGray = (double)yuvClr.rgbRed;
					stretcheds = (double)(tmpGray - lowerc) * ( (blimit - alimit) / (upperd - lowerc) ); // + alimit;
					if ( stretcheds < 0.0 )	stretcheds = 0.0;
					else if ( stretcheds > 255.0 ) stretcheds = 255.0;
					yuvClr.rgbRed = (BYTE)stretcheds;

					color = YUVtoRGB(yuvClr);
					SetPixelColor( x, y, color );
				}
			}
		} else {
			DWORD  j;
			for( j = 0; j < head.biClrUsed; j++ ){
				color = GetPaletteColor( (BYTE)j );
				tmpGray = RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
				if ( tmpGray < lowerc )	lowerc = tmpGray;
				if ( tmpGray > upperd )	upperd = tmpGray;
			}
			if (upperd==lowerc) return false;

			for( j = 0; j < head.biClrUsed; j++ ){

				color = GetPaletteColor( (BYTE)j );
				yuvClr = RGBtoYUV( color );

				// Stretch Luminance
				tmpGray = (double)yuvClr.rgbRed;
				stretcheds = (double)(tmpGray - lowerc) * ( (blimit - alimit) / (upperd - lowerc) ); // + alimit;
				if ( stretcheds < 0.0 )	stretcheds = 0.0;
				else if ( stretcheds > 255.0 ) stretcheds = 255.0;
				yuvClr.rgbRed = (BYTE)stretcheds;

				color = YUVtoRGB(yuvClr);
				SetPaletteColor( (BYTE)j, color );
			}
		}
	  }
	}
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
// HistogramEqualize function by <dave> : dave(at)posortho(dot)com
bool CxImage::HistogramEqualize()
{
	if (!pDib) return false;

    int histogram[256];
	int map[256];
	int equalize_map[256];
    int x, y, i, j;
	RGBQUAD color;
	RGBQUAD	yuvClr;
	unsigned int YVal, high, low;

	memset( &histogram, 0, sizeof(int) * 256 );
	memset( &map, 0, sizeof(int) * 256 );
	memset( &equalize_map, 0, sizeof(int) * 256 );
 
     // form histogram
	for(y=0; y < head.biHeight; y++){
		info.nProgress = (long)(50*y/head.biHeight);
		if (info.nEscape) break;
		for(x=0; x < head.biWidth; x++){
			color = GetPixelColor( x, y );
			YVal = (unsigned int)RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
			histogram[YVal]++;
		}
	}

	// integrate the histogram to get the equalization map.
	j = 0;
	for(i=0; i <= 255; i++){
		j += histogram[i];
		map[i] = j; 
	}

	// equalize
	low = map[0];
	high = map[255];
	if (low == high) return false;
	for( i = 0; i <= 255; i++ ){
		equalize_map[i] = (unsigned int)((((double)( map[i] - low ) ) * 255) / ( high - low ) );
	}

	// stretch the histogram
	if(head.biClrUsed == 0){ // No Palette
		for( y = 0; y < head.biHeight; y++ ){
			info.nProgress = (long)(50+50*y/head.biHeight);
			if (info.nEscape) break;
			for( x = 0; x < head.biWidth; x++ ){

				color = GetPixelColor( x, y );
				yuvClr = RGBtoYUV(color);

                yuvClr.rgbRed = (BYTE)equalize_map[yuvClr.rgbRed];

				color = YUVtoRGB(yuvClr);
				SetPixelColor( x, y, color );
			}
		}
	} else { // Palette
		for( i = 0; i < (int)head.biClrUsed; i++ ){

			color = GetPaletteColor((BYTE)i);
			yuvClr = RGBtoYUV(color);

            yuvClr.rgbRed = (BYTE)equalize_map[yuvClr.rgbRed];

			color = YUVtoRGB(yuvClr);
			SetPaletteColor( (BYTE)i, color );
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////
// HistogramNormalize function by <dave> : dave(at)posortho(dot)com
bool CxImage::HistogramNormalize()
{
	if (!pDib) return false;

	int histogram[256];
	int threshold_intensity, intense;
	int x, y, i;
	unsigned int normalize_map[256];
	unsigned int high, low, YVal;

	RGBQUAD color;
	RGBQUAD	yuvClr;

	memset( &histogram, 0, sizeof( int ) * 256 );
	memset( &normalize_map, 0, sizeof( unsigned int ) * 256 );
 
     // form histogram
	for(y=0; y < head.biHeight; y++){
		info.nProgress = (long)(50*y/head.biHeight);
		if (info.nEscape) break;
		for(x=0; x < head.biWidth; x++){
			color = GetPixelColor( x, y );
			YVal = (unsigned int)RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
			histogram[YVal]++;
		}
	}

	// find histogram boundaries by locating the 1 percent levels
	threshold_intensity = ( head.biWidth * head.biHeight) / 100;

	intense = 0;
	for( low = 0; low < 255; low++ ){
		intense += histogram[low];
		if( intense > threshold_intensity )	break;
	}

	intense = 0;
	for( high = 255; high != 0; high--){
		intense += histogram[ high ];
		if( intense > threshold_intensity ) break;
	}

	if ( low == high ){
		// Unreasonable contrast;  use zero threshold to determine boundaries.
		threshold_intensity = 0;
		intense = 0;
		for( low = 0; low < 255; low++){
			intense += histogram[low];
			if( intense > threshold_intensity )	break;
		}
		intense = 0;
		for( high = 255; high != 0; high-- ){
			intense += histogram [high ];
			if( intense > threshold_intensity )	break;
		}
	}
	if( low == high ) return false;  // zero span bound

	// Stretch the histogram to create the normalized image mapping.
	for(i = 0; i <= 255; i++){
		if ( i < (int) low ){
			normalize_map[i] = 0;
		} else {
			if(i > (int) high)
				normalize_map[i] = 255;
			else
				normalize_map[i] = ( 255 - 1) * ( i - low) / ( high - low );
		}
	}

	// Normalize
	if( head.biClrUsed == 0 ){
		for( y = 0; y < head.biHeight; y++ ){
			info.nProgress = (long)(50+50*y/head.biHeight);
			if (info.nEscape) break;
			for( x = 0; x < head.biWidth; x++ ){

				color = GetPixelColor( x, y );
				yuvClr = RGBtoYUV( color );

                yuvClr.rgbRed = (BYTE)normalize_map[yuvClr.rgbRed];

				color = YUVtoRGB( yuvClr );
				SetPixelColor( x, y, color );
			}
		}
	} else {
		for(i = 0; i < (int)head.biClrUsed; i++){

			color = GetPaletteColor( (BYTE)i );
			yuvClr = RGBtoYUV( color );

            yuvClr.rgbRed = (BYTE)normalize_map[yuvClr.rgbRed];

			color = YUVtoRGB( yuvClr );
 			SetPaletteColor( (BYTE)i, color );
		}
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////
// HistogramLog function by <dave> : dave(at)posortho(dot)com
bool CxImage::HistogramLog()
{
	if (!pDib) return false;

	//q(i,j) = 255/log(1 + |high|) * log(1 + |p(i,j)|);
    int x, y, i;
	RGBQUAD color;
	RGBQUAD	yuvClr;

	unsigned int YVal, high = 1;

    // Find Highest Luminance Value in the Image
	if( head.biClrUsed == 0 ){ // No Palette
		for(y=0; y < head.biHeight; y++){
			info.nProgress = (long)(50*y/head.biHeight);
			if (info.nEscape) break;
			for(x=0; x < head.biWidth; x++){
				color = GetPixelColor( x, y );
				YVal = (unsigned int)RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
				if (YVal > high ) high = YVal;
			}
		}
	} else { // Palette
		for(i = 0; i < (int)head.biClrUsed; i++){
			color = GetPaletteColor((BYTE)i);
			YVal = (unsigned int)RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
			if (YVal > high ) high = YVal;
		}
	}

	// Logarithm Operator
	double k = 255.0 / ::log( 1.0 + (double)high );
	if( head.biClrUsed == 0 ){
		for( y = 0; y < head.biHeight; y++ ){
			info.nProgress = (long)(50+50*y/head.biHeight);
			if (info.nEscape) break;
			for( x = 0; x < head.biWidth; x++ ){

				color = GetPixelColor( x, y );
				yuvClr = RGBtoYUV( color );
                
				yuvClr.rgbRed = (BYTE)(k * ::log( 1.0 + (double)yuvClr.rgbRed ) );

				color = YUVtoRGB( yuvClr );
				SetPixelColor( x, y, color );
			}
		}
	} else {
		for(i = 0; i < (int)head.biClrUsed; i++){

			color = GetPaletteColor( (BYTE)i );
			yuvClr = RGBtoYUV( color );

            yuvClr.rgbRed = (BYTE)(k * ::log( 1.0 + (double)yuvClr.rgbRed ) );
			
			color = YUVtoRGB( yuvClr );
 			SetPaletteColor( (BYTE)i, color );
		}
	}
 
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// HistogramRoot function by <dave> : dave(at)posortho(dot)com
bool CxImage::HistogramRoot()
{
	if (!pDib) return false;
	//q(i,j) = sqrt(|p(i,j)|);

    int x, y, i;
	RGBQUAD color;
	RGBQUAD	 yuvClr;
	double	dtmp;
	unsigned int YVal, high = 1;

     // Find Highest Luminance Value in the Image
	if( head.biClrUsed == 0 ){ // No Palette
		for(y=0; y < head.biHeight; y++){
			info.nProgress = (long)(50*y/head.biHeight);
			if (info.nEscape) break;
			for(x=0; x < head.biWidth; x++){
				color = GetPixelColor( x, y );
				YVal = (unsigned int)RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
				if (YVal > high ) high = YVal;
			}
		}
	} else { // Palette
		for(i = 0; i < (int)head.biClrUsed; i++){
			color = GetPaletteColor((BYTE)i);
			YVal = (unsigned int)RGB2GRAY(color.rgbRed, color.rgbGreen, color.rgbBlue);
			if (YVal > high ) high = YVal;
		}
	}

	// Root Operator
	double k = 128.0 / ::log( 1.0 + (double)high );
	if( head.biClrUsed == 0 ){
		for( y = 0; y < head.biHeight; y++ ){
			info.nProgress = (long)(50+50*y/head.biHeight);
			if (info.nEscape) break;
			for( x = 0; x < head.biWidth; x++ ){

				color = GetPixelColor( x, y );
				yuvClr = RGBtoYUV( color );

				dtmp = k * ::sqrt( (double)yuvClr.rgbRed );
				if ( dtmp > 255.0 )	dtmp = 255.0;
				if ( dtmp < 0 )	dtmp = 0;
                yuvClr.rgbRed = (BYTE)dtmp;

				color = YUVtoRGB( yuvClr );
				SetPixelColor( x, y, color );
			}
		}
	} else {
		for(i = 0; i < (int)head.biClrUsed; i++){

			color = GetPaletteColor( (BYTE)i );
			yuvClr = RGBtoYUV( color );

			dtmp = k * ::sqrt( (double)yuvClr.rgbRed );
			if ( dtmp > 255.0 )	dtmp = 255.0;
			if ( dtmp < 0 ) dtmp = 0;
            yuvClr.rgbRed = (BYTE)dtmp;

			color = YUVtoRGB( yuvClr );
 			SetPaletteColor( (BYTE)i, color );
		}
	}
 
	return true;
}
////////////////////////////////////////////////////////////////////////////////
#endif
