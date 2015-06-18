///A few useful helper functions

#ifndef FILTER_HELPER_HPP
#define FILTER_HELPER_HPP


/**
    * Convert a 32 bit image to a 24 bit one
    *
    * This centralizes a decent bit of code.
    * NOTE:  This takes width and height, and ASSUMES they are accurate!!!!!
    *
    * \param[in] src    A pointer to the input 32 bit RGB Pixel Array
    * \param[in] dst    A pointer to the output 24 bit RGB Pixel Array
    * \param[in] width  The image width (in pixels)
    * \param[in] height The height width (in pixels)
    */
inline void convert32To24(u32* src,u8* dst,unsigned int width, unsigned int height)
{
    //Need src as a single byte pointer for this to work
    u8 * u8src = (u8*)src;

    for(unsigned int y = 0; y < height ; y++)
    {
        for(unsigned int x = 0; x < width; x++)
        {
            //For each pixel copy r,g,b one byte at a time
            for(unsigned int i = 0; i<3; i++)
            {
                *dst++ = *u8src++;
            }
            //Skip the alpha channel
            u8src++;
        }
    }
}

/**
 * Get a pointer to the first pixel of a row inside an image
 *
 * NOTE:  If width or vertical_offset is too large, this function WILL produce an invalid pointer
 *
 * \param[in] image_pointer     A pointer to the start of an image
 * \param[in] width             The image's width
 * \param[in] vertical_offset   How many rows from the start of the image
 * \param[in] scale             How much larger the output image will be
 * \return                      A pointer to the first pixel in the chosen row
 */
inline u32 * GetVerticalOffset(u32 * image_pointer,unsigned int width,unsigned int vertical_offset,unsigned int scale=1)
{
    return image_pointer + (width * vertical_offset*scale*scale);
}

#endif //FILTER_HELPER_HPP
