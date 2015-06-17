///Base class for all filters

#ifndef FILTER_BASE_HPP
#define FILTER_BASE_HPP

#include "../common/Types.h"
#include <string>
#include <cstring>  //For memcpy
///Base class for all filters
class filter_base
{
private:
    //Need to give the filter a width,height at initialization
    filter_base();
    ///The filter's input width
    unsigned int width;
    ///The filter's input height
    unsigned int height;
    ///The filter's scale
    unsigned int scale;
protected:
    void setScale(unsigned int _scale) {scale=_scale;};
public:
    filter_base(unsigned int _width,unsigned int _height): width(_width),height(_height),scale(1) {}
    virtual ~filter_base() {}
    unsigned int getWidth() {return width;}
    unsigned int getHeight() {return height;}
    unsigned int getOutWidth() {return width*scale;}
    unsigned int getOutHeight() {return height*scale;}
    virtual std::string getName() {return "Dummy Filter";}
    virtual unsigned int getScale() {return scale;}
    virtual bool exists() {return false;}
    /**
     * Run the filter.
     *
     * All of the filters currently in use are designed to work with 32 bits (4 bytes) per pixel.
     * Of important note is that the output of the filter is scaled.  This means the output array must be scale * scale larger than the input array
     *
     * \param[in] srcPtr        A pointer to the input 32 bit RGBA Pixel Array
     * \param[in] dstPtr        A pointer to the output 32 bit RGBA Pixel Array
     */
    virtual void run(u32 *srcPtr,u32 *dstPtr)
    {
        //If the filter doesn't exist, then we still need to get the data to the output buffer
        std::memcpy(dstPtr,srcPtr, getWidth()*getHeight()*4);
    }
};

#endif  //FILTER_BASE_HPP
