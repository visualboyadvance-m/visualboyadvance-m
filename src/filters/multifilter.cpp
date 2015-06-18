/***********************************************************************
 * AUTHOR: Arthur Moore <arthur>
 *   FILE: .//multifilter.cpp
 *   DATE: Wed Jun 17 13:02:51 2015
 *  DESCR: 
 ***********************************************************************/
#include "multifilter.hpp"
#include "filter_base.hpp"
#include "filter_factory.hpp"

#include <iostream>

multifilter::multifilter(std::vector<std::string> filters, unsigned int X, unsigned int Y):
    filterNames(filters),numFilters(filters.size()),inX(X),inY(Y)
{
    std::cerr << "X="<<X<<", Y="<<Y<<std::endl;
    for(unsigned int i=0;i<numFilters;i++)
    {
        //Create, and append the new filter
        filter_base* aFilter = filter_factory::createFilter(filters[i],X,Y);
        filterPtrs.push_back(aFilter);
        //Get the size of the filter's output
        X=aFilter->getOutWidth();
        Y=aFilter->getOutHeight();
        std::cerr << "X="<<X<<", Y="<<Y<<std::endl;
        //Create and append the buffer with the new size
        u32* aBuffer = new u32[X*Y];
        filterBuffers.push_back(aBuffer);
    }
    //Remove the last filter buffer, the last filer outputs directly
    //Putting the null pointer on the end for use with setOutImage
    delete filterBuffers.back();
    filterBuffers.pop_back();
    filterBuffers.push_back(NULL);
}

multifilter::~multifilter()
{
    //Remove the last item from this vector, we don't manage external memory
    filterBuffers.pop_back();
    //Properly delete everything
    for (std::vector<filter_base*>::iterator it = filterPtrs.begin() ; it != filterPtrs.end(); ++it)
    {
        delete *it;
    }
    for (std::vector<u32*>::iterator it = filterBuffers.begin() ; it != filterBuffers.end(); ++it)
    {
        delete[] *it;
    }
}

unsigned int multifilter::getOutX()
{
    return filterPtrs.back()->getOutWidth();
}

unsigned int multifilter::getOutY()
{
    return filterPtrs.back()->getOutHeight();
}

void multifilter::setInImage(u32 *image)
{
    inImage=image;
}

void multifilter::setOutImage(u32 *image)
{
    //Remove the last buffer/image from the vector, and replace it with the current output image
    filterBuffers.pop_back();
    filterBuffers.push_back(image);
}

void multifilter::run()
{
    //Do the first one separate
    filterPtrs[0]->run(inImage,filterBuffers[0]);
    for(unsigned int i=1;i<numFilters;i++)
    {
        filterPtrs[i]->run(filterBuffers[i-1],filterBuffers[i]);
    }
    //Note:  Since, the output image is the last in the vector, don't need to worry about doing the last separate
}
