
#include "filter_factory.hpp"
#include "filters.hpp"
#include "new_interframe.hpp"

#include <cstdint>
typedef uint8_t u8;
typedef uint32_t u32;


// Function pointer type for a raw filter function
typedef void(*FilterFunc)(u8*, u32, u8*, u32, int, int);

typedef std::pair<std::string,FilterFunc> namedfilter;

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

///A named map of all the (original) filters
const std::map<std::string,FilterFunc> filterMap = makeFilterMap();

///Returns the filter's scaling factor
///TODO:  De hardcode this
static int GetFilterScale(std::string filterName)
{
    if(filterName == "HQ 4x" || filterName == "Simple 4x")
        return 4;
    if(filterName == "HQ 3x" || filterName == "Simple 3x")
        return 3;
    if(filterName == "None")
        return 1;
    return 2;
}

filter_base * filter_factory::createFilter(std::string filterName,unsigned int width,unsigned int height)
{

    // Initialize the two special filters
    // \HACK:  Shouldn't have to initialize anything
    // \WARNING:  These functions may cause the program to hang on exit
    if("HQ 2x" == filterName)
    {
        hq2x_init(32);
    }
    if("2xSaI" == filterName)
    {
        Init_2xSaI(32);
    }

    //Search the raw filters first
    std::map<std::string,FilterFunc>::const_iterator found = filterMap.find(filterName);
    //If we found the filter:
    if(found != filterMap.end()){
        return new raw_filter(filterName,found->second,GetFilterScale(filterName),width,height);
    }

    if("XBR 2x" == filterName)
    {
        return new xbr(width,height,2);
    }
    if("XBR 3x" == filterName)
    {
        return new xbr(width,height,3);
    }
    if("XBR 4x" == filterName)
    {
        return new xbr(width,height,4);
    }
    if("XBR 5x" == filterName)
    {
        return new xbr(width,height,5);
    }
    if("Smart interframe blending" == filterName)
    {
        return new SmartIB(width,height);
    }
    if("Interframe motion blur" == filterName)
    {
        return new MotionBlurIB(width,height);
    }
    if("Harsh"  == filterName)
    {
        return new harsh(width,height);
    }

    //If nothing found, just return a default filter
    return new filter_base(width,height);
}


