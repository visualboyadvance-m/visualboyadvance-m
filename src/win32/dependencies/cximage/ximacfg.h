#if !defined(__ximaCFG_h)
#define __ximaCFG_h

/////////////////////////////////////////////////////////////////////////////
// CxImage supported features
#define CXIMAGE_SUPPORT_ALPHA          1
#define CXIMAGE_SUPPORT_SELECTION      1
#define CXIMAGE_SUPPORT_TRANSFORMATION 1
#define CXIMAGE_SUPPORT_DSP            1
#define CXIMAGE_SUPPORT_LAYERS		   1
#define CXIMAGE_SUPPORT_INTERPOLATION  1

#define CXIMAGE_SUPPORT_DECODE	1
#define CXIMAGE_SUPPORT_ENCODE	1		//<vho><T.Peck>
#define	CXIMAGE_SUPPORT_WINDOWS 1
#define	CXIMAGE_SUPPORT_WINCE   0		//<T.Peck>

/////////////////////////////////////////////////////////////////////////////
// CxImage supported formats
#define CXIMAGE_SUPPORT_BMP 1
#define CXIMAGE_SUPPORT_GIF 1
#define CXIMAGE_SUPPORT_JPG 0
#define CXIMAGE_SUPPORT_PNG 0
#define CXIMAGE_SUPPORT_MNG 0
#define CXIMAGE_SUPPORT_ICO 1
#define CXIMAGE_SUPPORT_TIF 0
#define CXIMAGE_SUPPORT_TGA 1
#define CXIMAGE_SUPPORT_PCX 1
#define CXIMAGE_SUPPORT_WBMP 1
#define CXIMAGE_SUPPORT_WMF 1
#define CXIMAGE_SUPPORT_J2K 0		// Beta, use JP2
#define CXIMAGE_SUPPORT_JBG 0		// GPL'd see ../jbig/copying.txt & ../jbig/patents.htm

#define CXIMAGE_SUPPORT_JP2 0
#define CXIMAGE_SUPPORT_JPC 0
#define CXIMAGE_SUPPORT_PGX 0
#define CXIMAGE_SUPPORT_PNM 0
#define CXIMAGE_SUPPORT_RAS 0


/////////////////////////////////////////////////////////////////////////////
#define	CXIMAGE_MAX_MEMORY 256000000

#define CXIMAGE_ERR_NOFILE "null file handler"
#define CXIMAGE_ERR_NOIMAGE "null image!!!"

/////////////////////////////////////////////////////////////////////////////
//color to grey mapping <H. Muelner> <jurgene>
//#define RGB2GRAY(r,g,b) (((b)*114 + (g)*587 + (r)*299)/1000)
#define RGB2GRAY(r,g,b) (((b)*117 + (g)*601 + (r)*306) >> 10)

#endif
