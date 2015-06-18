/***********************************************************************
 * AUTHOR: Arthur Moore <arthur>
 *   FILE: .//multifilter.cpp
 *   DATE: Wed Jun 17 13:02:51 2015
 *  DESCR: 
 ***********************************************************************/
#include "multifilter.hpp"
#include "filter_base.hpp"
#include "filter_factory.hpp"

/***********************************************************************
 *  Method: multifilter::multifilter
 *  Params: std::vector<std::string> filters, unsigned int X, unsigned int, Y
 * Effects: 
 ***********************************************************************/
multifilter::multifilter(std::vector<std::string> filters, unsigned int X, unsigned int Y):
    filterNames(filters),numFilters(filters.size()),inX(X),inY(Y)
{
    //Do the first one separate
    filter_base * aFilter = filter_factory::createFilter(filters[0],X,Y);
    filterPtrs.push_back(aFilter);

    for(unsigned int i=1;i<numFilters;i++)
    {
        //Get the size of the last filter's output
        X=aFilter->getOutWidth();
        Y=aFilter->getOutHeight();
        //Create, and append the new filter
        aFilter = filter_factory::createFilter(filters[i],X,Y);
        filterPtrs.push_back(aFilter);
        //Create and append the buffers
        u32* aBuffer = new u32[X*Y];
        filterBuffers.push_back(aBuffer);
    }
}


/***********************************************************************
 *  Method: multifilter::getOutX
 *  Params: 
 * Effects: 
 ***********************************************************************/
unsigned int multifilter::getOutX()
{
    filterPtrs.back()->getOutWidth();
}


/***********************************************************************
 *  Method: multifilter::getOutY
 *  Params: 
 * Effects: 
 ***********************************************************************/
unsigned int multifilter::getOutY()
{
    filterPtrs.back()->getOutHeight();
}


/***********************************************************************
 *  Method: multifilter::setInImage
 *  Params: u32 *image
 * Effects: 
 ***********************************************************************/
void multifilter::setInImage(u32 *image)
{
    inImage=image;
}


/***********************************************************************
 *  Method: multifilter::setOutImage
 *  Params: u32 *image
 * Effects: 
 ***********************************************************************/
void multifilter::setOutImage(u32 *image)
{
    outImage=image;
}


/***********************************************************************
 *  Method: multifilter::run
 *  Params: 
 * Effects: 
 ***********************************************************************/
void multifilter::run()
{
    //Do the first one separate
    filterPtrs[0]->run(inImage,filterBuffers[0]);
    for(unsigned int i=1;i<(numFilters-1);i++)
    {
        filterPtrs[i]->run(filterBuffers[i-1],filterBuffers[i]);
    }
    //Do the last one separate
    filterPtrs.back()->run(filterBuffers.back(),outImage);
}
