#include "filters.hpp"

//Actually make the fitlermap (since C++11 doesn't work right now)
const std::map<std::string,FilterFunc> makeFilterMap()
{
    std::map<std::string,FilterFunc> tempMap;

    tempMap.insert(namedfilter("Pixelate",Pixelate32));
    tempMap.insert(namedfilter("Scanlines",Scanlines32));
    tempMap.insert(namedfilter("TV Mode",ScanlinesTV32));

    //These require Init_2xSaI(u32 BitFormat);
    tempMap.insert(namedfilter("Simple 2x",Simple2x32));
    tempMap.insert(namedfilter("Simple 3x",Simple3x32));
    tempMap.insert(namedfilter("Simple 4x",Simple4x32));

    tempMap.insert(namedfilter("Bilinear",Bilinear32));
    tempMap.insert(namedfilter("Bilinear Plus",BilinearPlus32));
    tempMap.insert(namedfilter("Advance MAME Scale2x",AdMame2x32));

    //These require Init_2xSaI(u32 BitFormat);
    tempMap.insert(namedfilter("2xSaI",_2xSaI32));
    tempMap.insert(namedfilter("Super 2xSaI",Super2xSaI32));
    tempMap.insert(namedfilter("Super Eagle",SuperEagle32));

    //These require calling hq2x_init first and whenever bpp changes
    tempMap.insert(namedfilter("HQ 2x",hq2x32));
    tempMap.insert(namedfilter("LQ 2x",lq2x32));

    tempMap.insert(namedfilter("HQ 3x",hq3x32));
    tempMap.insert(namedfilter("HQ 4x",hq4x32));

    //These require sdlStretchInit
    tempMap.insert(namedfilter("sdlStretch1x",sdlStretch1x));
    tempMap.insert(namedfilter("sdlStretch2x",sdlStretch2x));
    tempMap.insert(namedfilter("sdlStretch3x",sdlStretch3x));
    tempMap.insert(namedfilter("sdlStretch4x",sdlStretch4x));

    return tempMap;
}

const std::map<std::string,FilterFunc> filters::filterMap = makeFilterMap();

//Convert a 32 bit image to a 24 bit one
void convert32To24(u32* src,u8* dst,unsigned int width, unsigned int height)
{
    //Need src as a single byte pointer for this to work
    u8 * u8src = (u8*)src;

    for(unsigned int y = 0; y < height ; y++)
    {
        for(unsigned int x = 0; x < width; x++)
        {
            //For each pixel copy r,g,b one byte at a time
            for(unsigned int i = 0; i<3; i++)
            {
                *dst++ = *u8src++;
            }
            //Skip the alpha channel
            u8src++;
        }
    }
}

// Intialize color tables
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
    extern int systemRedShift    = 3;
    extern int systemGreenShift  = 11;
    extern int systemBlueShift   = 19;
    extern int RGB_LOW_BITS_MASK = 0x00010101;
#else
    extern int systemRedShift    = 27;
    extern int systemGreenShift  = 19;
    extern int systemBlueShift   = 11;
    extern int RGB_LOW_BITS_MASK = 0x01010100;
#endif
