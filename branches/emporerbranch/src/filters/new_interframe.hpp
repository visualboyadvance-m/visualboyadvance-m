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
    
    //The filter's height
    unsigned int height;
    
    //Don't need to calculate these every time (based off width)
    unsigned int horiz_bytes;
    //unsigned int horiz_bytes_out;
    //Children need this pre-calculated data, but it's NOT public
protected:
    unsigned int get_horiz_bytes() {return horiz_bytes;}
    //unsigned int get_horiz_bytes_out() {return horiz_bytes_out;}
public:
    interframe_filter(): width(0) {}
    virtual std::string getName() {return "Dummy Filter";}
    virtual int getScale() {return 0;}
    
    //Set the number of pixels per horizontal row
    //Always use this after initialization if using the new run function.
    void setWidth(unsigned int _width);
    unsigned int getWidth() {return width;}
    
    //Set the number of horizontal rows in the image
    //Always use this after initialization if using the new run function.
    void setHeight(unsigned int _height){height=_height;}
    unsigned int getHeight() {return height;}
    
    //New smarter Interframe function
    virtual void run(u8 *srcPtr, int starty, int height);
};


// Interframe blending filters (These are the 32 bit versions)
// definitely not thread safe by default
// added band_lower param to provide offset into accum buffers

class SmartIB : public interframe_filter
{
private:
    u8 *frm1 = NULL;
    u8 *frm2 = NULL;
    u8 *frm3 = NULL;
public:
    SmartIB();
    ~SmartIB();
    std::string getName() {return "SmartIB";}
    void run(u8 *srcPtr, int starty, int height);
};

class MotionBlurIB : public interframe_filter
{
private:
    u8 *frm1 = NULL;
    u8 *frm2 = NULL;
    u8 *frm3 = NULL;
public:
    MotionBlurIB();
    ~MotionBlurIB();
    std::string getName() {return "MotionBlurIB";}
    void run(u8 *srcPtr, int starty, int height);
};



#endif  //NEW_INTERFRAME_HPP

