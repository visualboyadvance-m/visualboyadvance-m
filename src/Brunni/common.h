#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include "VirtualFile.h"
#include <windows.h>

#define oslMin(x, y)				(((x)<(y))?(x):(y))
#define oslMax(x, y)				(((x)>(y))?(x):(y))

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#include "config.h"

//Dans gbGfx.cpp
extern void tilePalReinit();
void tilePalInit();


