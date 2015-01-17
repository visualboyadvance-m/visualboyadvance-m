#include "filters.hpp"

//Helper function for creating filterMap
namedfilter InsertFilter(std::string filterName, FilterFunc thirtyTwoBitFilter, FilterFunc sixteenBitFilter)
{
    filterpair filters = std::make_pair<FilterFunc,FilterFunc>(thirtyTwoBitFilter,sixteenBitFilter);
    return std::make_pair<std::string,filterpair>(filterName,filters) ;
}

//Actually make the fitlermap (since C++11 doesn't work right now)
const std::map<std::string,filterpair> makeFilterMap()
{
    InsertFilter("Pixelate",Pixelate32,Pixelate);
    InsertFilter("Scanlines",Scanlines32,Scanlines);
    InsertFilter("ScanlinesTV",ScanlinesTV32,ScanlinesTV);

    //These require Init_2xSaI(u32 BitFormat);
    InsertFilter("Simple2x",Simple2x32,Simple2x16);
    InsertFilter("Simple3x",Simple3x32,Simple3x16);
    InsertFilter("Simple4x",Simple4x32,Simple4x16);

    InsertFilter("Bilinear",Bilinear32,Bilinear);
    InsertFilter("BilinearPlus",BilinearPlus32,BilinearPlus);
    InsertFilter("AdMame2x",AdMame2x32,AdMame2x);

    //These require Init_2xSaI(u32 BitFormat);
    InsertFilter("_2xSaI",_2xSaI32,_2xSaI);
    InsertFilter("Super2xSaI",Super2xSaI32,Super2xSaI);
    InsertFilter("SuperEagle",SuperEagle32,SuperEagle);

    //These require calling hq2x_init first and whenever bpp changes
    InsertFilter("hq2x",hq2x32,hq2x);
    InsertFilter("lq2x",lq2x32,lq2x);

    InsertFilter("hq3x",hq3x32,hq3x16);
    InsertFilter("hq4x",hq4x32,hq4x16);

    //These require sdlStretchInit
    InsertFilter("sdlStretch1x",sdlStretch1x,sdlStretch1x);
    InsertFilter("sdlStretch2x",sdlStretch2x,sdlStretch2x);
    InsertFilter("sdlStretch3x",sdlStretch3x,sdlStretch3x);
    InsertFilter("sdlStretch4x",sdlStretch4x,sdlStretch4x);
}

const std::map<std::string,filterpair> filters::filterMap = makeFilterMap();
