/// Filters (Using propper C++ classes instead of statics)

#ifndef NEW_INTERFRAME_HPP
#define NEW_INTERFRAME_HPP

#include "../common/Types.h"
#include <string>

class interframe_filter
{
private:
    ///The filter's width
    unsigned int width;
    ///The filter's height
    unsigned int height;
    ///The internal scale
//     int myScale;
    ///Don't need to calculate these every time (based off width)
//     unsigned int horiz_bytes;
//     unsigned int horiz_bytes_out;
//Children need this pre-calculated data, but it's NOT public
// protected:
//     unsigned int get_horiz_bytes() {return horiz_bytes;}
//     unsigned int get_horiz_bytes_out() {return horiz_bytes_out;}
public:
    interframe_filter(unsigned int _width=0,unsigned int _height=0): width(_width),height(_height) {}
    virtual std::string getName() {return "Dummy Filter";}
    virtual int getScale() {return 0;}
    unsigned int getWidth() {return width;}
    unsigned int getHeight() {return height;}
    ///New smarter Interframe function
    virtual void run(u32 *srcPtr, unsigned int num_threads=1,unsigned int thread_number=0) {}
    virtual bool exists() {return false;}
};


// Interframe blending filters (These are the 32 bit versions)
// definitely not thread safe by default
// added band_lower param to provide offset into accum buffers

class SmartIB : public interframe_filter
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
    void run(u32 *srcPtr, unsigned int num_threads=1,unsigned int thread_number=0);
    bool exists() {return true;}
};

class MotionBlurIB : public interframe_filter
{
private:
    u32 *frm1;
    //Must enter width and height at filter initialization
    MotionBlurIB();
public:
    MotionBlurIB(unsigned int _width,unsigned int _height);
    ~MotionBlurIB();
    std::string getName() {return "MotionBlurIB";}
    void run(u32 *srcPtr, unsigned int num_threads=1,unsigned int thread_number=0);
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
    static interframe_filter * createIFB(ifbfunc filter_select,unsigned int width,unsigned int height)
    {
        switch(filter_select) {
            case IFB_SMART:
                return new SmartIB(width,height);
                break;
            case IFB_MOTION_BLUR:
                return new MotionBlurIB(width,height);
                break;
            default:
                return new interframe_filter();
                break;
        }
    }
};

#endif  //NEW_INTERFRAME_HPP

