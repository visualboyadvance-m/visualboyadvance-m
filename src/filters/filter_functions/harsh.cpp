#include "../filters.hpp"

///Arthur's harsh filter
///All colors are either off or full color
void harsh::run(u32 *srcPtr,u32 *dstPtr)
{
    unsigned int numPixels = this->getWidth() * this->getHeight();
    for(unsigned int i=0;i<numPixels;++i)
    {
        u8* pixelIn = (u8*)(&srcPtr[i]);
        u8* pixelOut = (u8*)(&dstPtr[i]);
        //For each pixel set it to 0x00 or 0xff
        //Last is an alpha channel which is ignored
        for(unsigned int i = 0; i<3; i++)
        {
            if(pixelIn[i] < 128)
            {
                pixelOut[i] = 0;
            }
            else
            {
                pixelOut[i] = 0xFF;
            }
        }
    }

}
