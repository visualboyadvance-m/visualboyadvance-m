#ifndef PSP_CONFIG_H
#define PSP_CONFIG_H

#include "common.h"

typedef unsigned long OSL_COLOR;

//For game boy ColorIt.
#define NB_PALETTES 48

#ifdef RGB
	#undef RGB
#endif

/** Pixelformats (color types / modes). */
enum OSL_PIXELFORMATS		{
	OSL_PF_5650,					//!< 16 bits, 5 bits per component, except green which has 6, no alpha
	OSL_PF_5551,					//!< 15 bits, 5 bits per component, 1 alpha bit
	OSL_PF_4444,					//!< 12 bits, 4 bits per component, 4 alpha bits
	OSL_PF_8888,					//!< 32 bits, 8 bits per component, 8 alpha bits
	OSL_PF_4BIT,					//!< Paletted format, 4 bits (16 colors), cannot be set as drawbuffer
	OSL_PF_8BIT						//!< Paletted format, 8 bits (256 colors), cannot be set as drawbuffer
};

/** Creates a 32-bit opaque color. */
#define RGB(r,v,b)		((r) | ((v)<<8) | ((b)<<16) | (0xff<<24))
/** Creates a 32-bit color. 32-bit colors is what all OSLib calls asking for a color want. Use RGB if you don't want to bother about alpha (semi transparency). */
#define RGBA(r,v,b,a)	((r) | ((v)<<8) | ((b)<<16) | ((a)<<24))
/** Creates a 12-bit color with 3 coefficients (red, green, blue). Alpha is set to the maximum (opaque). The r, v, b values can be from 0 to 255, they're divided to match the 12-bit pixel format. */
#define RGB12(r,v,b)	((((b)>>4)<<8) | (((v)>>4)<<4) | ((r)>>4) | (0xf<<12))
/** Creates a 12-bit color with 4 coefficients (red, green, blue, alpha). */
#define RGBA12(r,v,b,a)	((((a)>>4)<<12) | (((b)>>4)<<8) | (((v)>>4)<<4) | ((r)>>4))
/** Creates a 15-bit opaque color. */
#define RGB15(r,v,b)	((((b)>>3)<<10) | (((v)>>3)<<5) | ((r)>>3) | (1<<15))
/** Creates a 15-bit color with alpha. */
#define RGBA15(r,v,b,a)	((((a)>>7)<<15) | (((b)>>3)<<10) | (((v)>>3)<<5) | ((r)>>3))
/** Creates a 16-bit color. */
#define RGB16(r,v,b)	((((b)>>3)<<11) | (((v)>>2)<<5) | ((r)>>3))

#define oslRgbaGet8888(data, r, g, b, a)		((r)=((data)&0xff), (g)=(((data)>>8)&0xff), (b)=(((data)>>16)&0xff), (a)=(((data)>>24)&0xff))
#define oslRgbaGet4444(data, r, g, b, a)		((r)=((data)&0xf)<<4, (g)=(((data)>>4)&0xf)<<4, (b)=(((data)>>8)&0xf)<<4, (a)=(((data)>>12)&0xf)<<4)
#define oslRgbaGet5551(data, r, g, b, a)		((r)=((data)&0x1f)<<3, (g)=(((data)>>5)&0x1f)<<3, (b)=(((data)>>10)&0x1f)<<3, (a)=(((data)>>15)&0x1)<<7)
#define oslRgbGet5650(data, r, g, b)			((r)=((data)&0x1f)<<3, (g)=(((data)>>5)&0x3f)<<2, (b)=(((data)>>11)&0x1f)<<3)

#define oslRgbaGet4444f(data, r, g, b, a)		((r)=((data)&0xf)<<4 | ((data)&0xf), (g)=(((data)>>4)&0xf)<<4 | (((data)>>4)&0xf), (b)=(((data)>>8)&0xf)<<4 | (((data)>>8)&0xf), (a)=(((data)>>12)&0xf)<<4 | (((data)>>12)&0xf))
#define oslRgbaGet5551f(data, r, g, b, a)		((r)=((data)&0x1f)<<3 | ((data)&0x1f)>>2, (g)=(((data)>>5)&0x1f)<<3 | (((data)>>5)&0x1f)>>2, (b)=(((data)>>10)&0x1f)<<3 | (((data)>>10)&0x1f)>>2, (a)=(((data)>>15)&0x1)*255)
#define oslRgbGet5650f(data, r, g, b)			((r)=((data)&0x1f)<<3 | ((data)&0x1f)>>2, (g)=(((data)>>5)&0x3f)<<2 | (((data)>>5)&0x3f)>>4, (b)=(((data)>>11)&0x1f)<<3 | (((data)>>10)&0x1f)>>2)

#define numberof(a)		(sizeof(a)/sizeof(*(a)))

typedef struct		{
	char *nom;
	void *handle;
	unsigned char type, flags;
	void *handle2;
} IDENTIFICATEUR;

typedef struct		{
	int taille;
	const IDENTIFICATEUR *objet;
} INFOS_OBJET;

typedef struct		{
	int taille;
	int type;
	const void *tableau;
} INFOS_TABLEAU;

//Dans un tableau, toutes les chaînes ont cette taille!!!
#define TAILLE_MAX_CHAINES 512
enum {SAVETYPE_DEFAULT, SAVETYPE_GAME, SAVETYPE_CHANGESONLY};

extern int gblIndice;
extern char menuDefaultExecute[TAILLE_MAX_CHAINES];
extern int gblModeColorIt;
extern char gblColorItPaletteFileName[MAX_PATH];
extern int gblConfigAutoShowCrc;

extern int ExecScript(VIRTUAL_FILE *f, char *fonction, char *command);
extern void SetVariableValue(const IDENTIFICATEUR *id, char *ptr);
extern char *GetVariableValue(const IDENTIFICATEUR *id, int withQuotes);
extern const IDENTIFICATEUR *GetVariableAddress(char *str);
extern int OuvreFichierConfig(char *nom_fichier, char *fonction);
extern char *GetChaine(char **dest);
//extern u32 GetAddedColor(int color, int value);
extern void ScriptSave(const char *filename, int type);
extern int oslConvertColor(int pfDst, int pfSrc, int color);
extern int CheckFichierPalette(char *file);
extern void OuvreFichierPalette(int crc, char *fonction);
extern u8 *getGbExtVramAddr();
extern char *extractFilePath(char *file, char *dstPath, int level);

#endif
