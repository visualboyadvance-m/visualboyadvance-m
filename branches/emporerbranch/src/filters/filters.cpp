#include "filters.hpp"

//Helper function for creating filterMap
namedfilter InsertFilter(std::string filterName, FilterFunc thirtyTwoBitFilter, FilterFunc sixteenBitFilter)
{
    filterpair filters = std::make_pair<FilterFunc,FilterFunc>(thirtyTwoBitFilter,sixteenBitFilter);
    return std::make_pair<std::string,filterpair>(filterName,filters) ;
}

//Actually make the fitlermap (since C++11 doesn't work right now)
std::map<std::string,filterpair> makeFilterMap()
{
    std::map<std::string,filterpair> tempMap;
    
    tempMap.insert(InsertFilter("Pixelate",Pixelate32,Pixelate));
    tempMap.insert(InsertFilter("Scanlines",Scanlines32,Scanlines));
    tempMap.insert(InsertFilter("TV Mode",ScanlinesTV32,ScanlinesTV));

    //These require Init_2xSaI(u32 BitFormat);
    tempMap.insert(InsertFilter("Simple 2x",Simple2x32,Simple2x16));
    tempMap.insert(InsertFilter("Simple 3x",Simple3x32,Simple3x16));
    tempMap.insert(InsertFilter("Simple 4x",Simple4x32,Simple4x16));

    tempMap.insert(InsertFilter("Bilinear",Bilinear32,Bilinear));
    tempMap.insert(InsertFilter("Bilinear Plus",BilinearPlus32,BilinearPlus));
    tempMap.insert(InsertFilter("Advance MAME Scale2x",AdMame2x32,AdMame2x));

    //These require Init_2xSaI(u32 BitFormat);
    tempMap.insert(InsertFilter("2xSaI",_2xSaI32,_2xSaI));
    tempMap.insert(InsertFilter("Super 2xSaI",Super2xSaI32,Super2xSaI));
    tempMap.insert(InsertFilter("Super Eagle",SuperEagle32,SuperEagle));

    //These require calling hq2x_init first and whenever bpp changes
    tempMap.insert(InsertFilter("hq2x",hq2x32,hq2x));
    tempMap.insert(InsertFilter("lq2x",lq2x32,lq2x));

    tempMap.insert(InsertFilter("HQ 3x",hq3x32,hq3x16));
    tempMap.insert(InsertFilter("HQ 4x",hq4x32,hq4x16));

    //These require sdlStretchInit
    tempMap.insert(InsertFilter("sdlStretch1x",sdlStretch1x,sdlStretch1x));
    tempMap.insert(InsertFilter("sdlStretch2x",sdlStretch2x,sdlStretch2x));
    tempMap.insert(InsertFilter("sdlStretch3x",sdlStretch3x,sdlStretch3x));
    tempMap.insert(InsertFilter("sdlStretch4x",sdlStretch4x,sdlStretch4x));
    
    return tempMap;
}

const std::map<std::string,filterpair> filters::filterMap = makeFilterMap();