// ximainfo.cpp : main attributes
/* 03/10/2004 v1.00 - Davide Pizzolato - www.xdp.it
 * CxImage version 5.99c 17/Oct/2004
 */

#include "ximage.h"

////////////////////////////////////////////////////////////////////////////////
/**
 * \return the color used for transparency, and/or for background color
 */
RGBQUAD	CxImage::GetTransColor()
{
	if (head.biBitCount<24 && info.nBkgndIndex != -1) return GetPaletteColor((BYTE)info.nBkgndIndex);
	return info.nBkgndColor;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Gets the index used for transparency. Returns -1 for no transparancy.
 */
long CxImage::GetTransIndex() const
{
	return info.nBkgndIndex;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Sets the index used for transparency with 1, 4 and 8 bpp images. Set to -1 to remove the effect.
 */
void CxImage::SetTransIndex(long idx)
{
	info.nBkgndIndex = idx;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Sets the color used for transparency with 24 bpp images.
 * You must call SetTransIndex(0) to enable the effect, SetTransIndex(-1) to disable it.
 */
void CxImage::SetTransColor(RGBQUAD rgb)
{
	rgb.rgbReserved=0;
	info.nBkgndColor = rgb;
}
////////////////////////////////////////////////////////////////////////////////
bool CxImage::IsTransparent() const
{
	return info.nBkgndIndex>=0; // <vho>
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Returns true if the image has 256 colors or less.
 */
bool CxImage::IsIndexed() const
{
	return head.biClrUsed!=0;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return 1 = indexed, 2 = RGB, 4 = RGBA
 */
BYTE CxImage::GetColorType()
{
	BYTE b = (BYTE)((head.biBitCount>8) ? 2 /*COLORTYPE_COLOR*/ : 1 /*COLORTYPE_PALETTE*/);
#if CXIMAGE_SUPPORT_ALPHA
	if (AlphaIsValid()) b = 4 /*COLORTYPE_ALPHA*/;
#endif //CXIMAGE_SUPPORT_ALPHA
	return b;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return Resolution for TIFF, JPEG, PNG and BMP formats.
 */
long CxImage::GetXDPI() const
{
	return info.xDPI;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return Resolution for TIFF, JPEG, PNG and BMP formats.
 */
long CxImage::GetYDPI() const
{
	return info.yDPI;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Set resolution for TIFF, JPEG, PNG and BMP formats.
 */
void CxImage::SetXDPI(long dpi)
{
	if (dpi<=0) dpi=96;
	info.xDPI = dpi;
	head.biXPelsPerMeter = (long) floor(dpi * 10000.0 / 254.0 + 0.5);
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Set resolution for TIFF, JPEG, PNG and BMP formats.
 */
void CxImage::SetYDPI(long dpi)
{
	if (dpi<=0) dpi=96;
	info.yDPI = dpi;
	head.biYPelsPerMeter = (long) floor(dpi * 10000.0 / 254.0 + 0.5);
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \sa SetFlags
 */
DWORD CxImage::GetFlags() const
{
	return info.dwFlags;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Image flags, for future use
 * \param flags
 *  - 0x??00000 = reserved for 16 bit, CMYK, multilayer
 *  - 0x00??0000 = blend modes
 *  - 0x0000???? = layer id or user flags
 *
 * \param bLockReservedFlags protects the "reserved" and "blend modes" flags 
 */
void CxImage::SetFlags(DWORD flags, bool bLockReservedFlags)
{
	if (bLockReservedFlags) info.dwFlags = flags & 0x0000ffff;
	else info.dwFlags = flags;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \sa SetCodecOption
 */
DWORD CxImage::GetCodecOption(DWORD imagetype)
{
	if (imagetype<CMAX_IMAGE_FORMATS){
		if (imagetype==0){
			imagetype = GetType();
		}
		return info.dwCodecOpt[imagetype];
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Encode option for GIF, TIF and JPG.
 * - GIF : 0 = LZW (default), 1 = none, 2 = RLE.
 * - TIF : 0 = automatic (default), or a valid compression code as defined in "tiff.h" (COMPRESSION_NONE = 1, COMPRESSION_CCITTRLE = 2, ...)
 * - JPG : valid values stored in enum CODEC_OPTION ( ENCODE_BASELINE = 0x01, ENCODE_PROGRESSIVE = 0x10, ...)
 *
 * \return true if everything is ok
 */
bool CxImage::SetCodecOption(DWORD opt, DWORD imagetype)
{
	if (imagetype<CMAX_IMAGE_FORMATS){
		if (imagetype==0){
			imagetype = GetType();
		}
		info.dwCodecOpt[imagetype] = opt;
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return internal hDib object..
 */
void* CxImage::GetDIB() const
{
	return pDib;
}
////////////////////////////////////////////////////////////////////////////////
DWORD CxImage::GetHeight() const
{
	return head.biHeight;
}
////////////////////////////////////////////////////////////////////////////////
DWORD CxImage::GetWidth() const
{
	return head.biWidth;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return DWORD aligned width of the image.
 */
DWORD CxImage::GetEffWidth() const
{
	return info.dwEffWidth;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return 2, 16, 256; 0 for RGB images.
 */
DWORD CxImage::GetNumColors() const
{
	return head.biClrUsed;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return: 1, 4, 8, 24.
 */
WORD CxImage::GetBpp() const
{
	return head.biBitCount;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return original image format
 * \sa ENUM_CXIMAGE_FORMATS.
 */
DWORD CxImage::GetType() const
{
	return info.dwType;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return current frame delay in milliseconds. Only for GIF and MNG formats.
 */
DWORD CxImage::GetFrameDelay() const
{
	return info.dwFrameDelay;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Sets current frame delay. Only for GIF format.
 * \param d = delay in milliseconds
 */
void CxImage::SetFrameDelay(DWORD d)
{
	info.dwFrameDelay=d;
}
////////////////////////////////////////////////////////////////////////////////
void CxImage::GetOffset(long *x,long *y)
{
	*x=info.xOffset;
	*y=info.yOffset;
}
////////////////////////////////////////////////////////////////////////////////
void CxImage::SetOffset(long x,long y)
{
	info.xOffset=x;
	info.yOffset=y;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \sa SetJpegQuality
 */
BYTE CxImage::GetJpegQuality() const
{
	return info.nQuality;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * quality level for JPEG and JPEG2000
 * \param q: can be from 0 to 100
 */
void CxImage::SetJpegQuality(BYTE q){
	info.nQuality = q;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \sa SetJpegScale
 */
BYTE CxImage::GetJpegScale() const
{
	return info.nJpegScale;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * scaling down during JPEG decoding valid numbers are 1, 2, 4, 8
 * \author [ignacio]
 */
void CxImage::SetJpegScale(BYTE q){
	info.nJpegScale = q;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Used to monitor the slow loops.
 * \return value is from 0 to 100.
 * \sa SetProgress
 */
long CxImage::GetProgress() const
{
	return info.nProgress;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return the escape code.
 * \sa SetEscape
 */
long CxImage::GetEscape() const
{
	return info.nEscape;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Forces the value of the internal progress variable.
 * \param p should be from 0 to 100.
 * \sa GetProgress
 */
void CxImage::SetProgress(long p)
{
	info.nProgress = p;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Used to quit the slow loops or the codecs.
 * - SetEscape(-1) before Decode forces the function to exit, right after  
 *   the image width and height are available ( for bmp, jpg, gif, tif )
 */
void CxImage::SetEscape(long i)
{
	info.nEscape = i;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Checks if the image is correctly initializated.
 */
bool CxImage::IsValid() const
{
	return pDib!=0;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * True if the image is enabled for painting.
 */
bool CxImage::IsEnabled() const
{
	return info.bEnabled;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Enables/disables the image.
 */
void CxImage::Enable(bool enable)
{
	info.bEnabled=enable;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * This function must be used after a Decode() / Load() call.
 * Use the sequence SetFrame(-1); Load(...); GetNumFrames();
 * to get the number of images without loading the first image.
 * \return the number of images in the file.
 */
long CxImage::GetNumFrames() const
{
	return info.nNumFrames;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return the current selected image (zero-based index).
 */
long CxImage::GetFrame() const
{
	return info.nFrame;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Sets the image number that the next Decode() / Load() call will load
 */
void CxImage::SetFrame(long nFrame){
	info.nFrame=nFrame;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * Returns the last reported error.
 */
const char* CxImage::GetLastError()
{
	return info.szLastError;
}
////////////////////////////////////////////////////////////////////////////////
/**
 * \return A.BBCCCDDDD
 *  - A = main version
 *  - BB = main revision
 *  - CCC = minor revision (letter)
 *  - DDDD = experimental revision
 */
const float CxImage::GetVersionNumber()
{
	return 5.99003f;
}
////////////////////////////////////////////////////////////////////////////////
const TCHAR* CxImage::GetVersion()
{
	static const TCHAR CxImageVersion[] = _T("CxImage 5.99c");
	return (CxImageVersion);
}
////////////////////////////////////////////////////////////////////////////////
