/// Filters (Using propper C++ classes instead of statics)

#ifndef NEW_INTERFRAME_HPP
#define NEW_INTERFRAME_HPP

#include "../common/Types.h"
#include <string>
#include "filter_base.hpp"

// Interframe blending filters (These are the 32 bit versions)
// definitely not thread safe by default
// added band_lower param to provide offset into accum buffers

class SmartIB : public filter_base
{
private:
    u32 *frm1;
    u32 *frm2;
    u32 *frm3;
    SmartIB();
public:
    SmartIB(unsigned int _width,unsigned int _height);
    ~SmartIB();
    std::string getName() {return "SmartIB";}
    void run(u32 *srcPtr,u32 *dstPtr);
    bool exists() {return true;}
};

class MotionBlurIB : public filter_base
{
private:
    u32 *frm1;
    //Must enter width and height at filter initialization
    MotionBlurIB();
public:
    MotionBlurIB(unsigned int _width,unsigned int _height);
    ~MotionBlurIB();
    std::string getName() {return "MotionBlurIB";}
    void run(u32 *srcPtr,u32 *dstPtr);
    bool exists() {return true;}
};

///Use this to select/create the filter to use
class interframe_factory
{
public:
    static filter_base * createIFB(std::string filterName,unsigned int width,unsigned int height)
    {
        if(filterName == "Smart interframe blending")
        {
            return new SmartIB(width,height);
        }
        else if(filterName == "Interframe motion blur")
        {
            return new MotionBlurIB(width,height);
        }
        return new filter_base(width,height);
    }
};

#endif  //NEW_INTERFRAME_HPP
