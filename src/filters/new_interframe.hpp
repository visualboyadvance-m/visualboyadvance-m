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

//WX
enum ifbfunc {
    // this order must match order of option enum and selector widget
    IFB_NONE, IFB_SMART, IFB_MOTION_BLUR
};

///Use this to select/create the filter to use
class interframe_factory
{
public:
    static filter_base * createIFB(ifbfunc filter_select,unsigned int width,unsigned int height)
    {
        switch(filter_select)
        {
            case IFB_SMART:
                return new SmartIB(width,height);
                break;
            case IFB_MOTION_BLUR:
                return new MotionBlurIB(width,height);
                break;
            default:
                return new filter_base(width,height);
                break;
        }
    }
    static bool exists(ifbfunc filter_select)
    {
       switch(filter_select)
       {
            case IFB_SMART:
            case IFB_MOTION_BLUR:
                return true;
                break;
            default:
                return false;
                break;
       }
    }
};

#endif  //NEW_INTERFRAME_HPP
