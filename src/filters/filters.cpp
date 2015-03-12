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
//     //Each pixel is 3 bytes, which is NOT a common unit, so have to use u8 and manually multiply
//     for(unsigned int i=0;i<width*height;i++)
//     {
//         dst[i*3]=((src[i])&0xFF) | ((src[i]>>8)&0xFF) | ((src[i]>>16)&0xFF);
//     }

    for(unsigned int y = 0; y < height ; y++) {
        for(unsigned int x = 0; x < width; x++, src++) {
            *dst++ = *src;
            *dst++ = *src >> 8;
            *dst++ = *src >> 16;
        }
//         ++src; // skip rhs border
    }
}
