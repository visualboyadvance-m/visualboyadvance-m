#include "filter_helper.hpp"

//Convert a 32 bit image to a 24 bit one
void convert32To24(u32* src,u8* dst,unsigned int width, unsigned int height)
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

//Get a pointer to the first pixel of a row inside an image
u32 * GetVerticalOffset(u32 * image_pointer,unsigned int width,unsigned int vertical_offset,unsigned int scale=1)
{
    return image_pointer + (width * vertical_offset*scale*scale);
}

//Split an image into multiple sections, with each section being a certain number of rows of the original image
std::vector<raw_image> splitImage(raw_image inImage, unsigned int number)
{
    std::vector<raw_image> outImages;
    raw_image outImage;

    //Handle if the user accidentally gave us a zero for number
    if(number <= 0)
    {
        number = 1;
    }

    //The height of each image
    unsigned int band_height = inImage.height/number;
    //Don't let the last few rows get lost
    unsigned int remainder_height = inImage.height%number;

    //Start by setting the out pointer to the same as the in pointer
    outImage.image = inImage.image;
    //All images are the same width
    outImage.width=inImage.width;
    //All but the last are the same height
    outImage.height=band_height;

    //Process all but the last image
    for(unsigned int i=0;i<(number-1);++i)
    {
        //Store the output image
        outImages.push_back(outImage);
        //Prepare for the next one (move the pointer band_height rows down)
        outImage.image=outImage.image+(outImage.width*band_height);
    }

    //Process the last image (including the remainder)
    outImage.height=band_height+remainder_height;
    //Pointer manipulation was handled in the for loop
    outImages.push_back(outImage);

    //Give the user the results
    return outImages;
}
