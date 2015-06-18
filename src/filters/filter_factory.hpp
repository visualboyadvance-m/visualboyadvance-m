///Use this to select/create the filter to use
#ifndef FILTER_FACTORY_HPP
#define FILTER_FACTORY_HPP

#include <map>
#include <string>

#include "filter_base.hpp"

class filter_factory
{
public:
    ///Returns an instance of a filter of filterName
    ///If the filter doesn't exist, it returns a dummy filter instead
    static filter_base* createFilter(std::string filterName,unsigned int width,unsigned int height);
};

#endif //FILTER_FACTORY_HPP
