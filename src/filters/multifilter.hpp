
#ifndef MULTIFILTER_HPP
#define MULTIFILTER_HPP

#include <vector>
#include <string>

#include <cstdint>
typedef uint32_t u32;

class filter_base;

class multifilter
{
private:
    ///Names of all the filters to be used
    std::vector<std::string> filterNames;
    ///The actual filters themselves
    std::vector<filter_base*> filterPtrs;
    ///The buffers between each filter (note that the first element is the buffer coming out of the first filter)
    std::vector<u32*> filterBuffers;
    ///Number of filters
    unsigned int numFilters;
    ///Size of the input image
    unsigned int inX, inY;
    ///Size of the final output image
    unsigned int outX, outY;
    ///Input image buffer
    u32* inImage;
public:
    multifilter(std::vector<std::string> filters,unsigned int X,unsigned int Y);
    ~multifilter();
    unsigned int getOutX();
    unsigned int getOutY();
    void setInImage(u32* image);
    void setOutImage(u32* image);
    void run();
};

/*
 * Usage:
 * std::vector<std::string> filters = {"filter1","filter2","filter3"};
 * multifilter myFilters(filters,32,32);
 * myFilters.setInImage(data);
 * u32* outImage=new u32 [myFilters.getOutX()*myFilters.getOutY()];
 * myFilters.setOutImage(outImage);
 * myFilters.run();
 */


#endif //MULTIFILTER_HPP
