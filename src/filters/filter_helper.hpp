///A few useful helper functions

#ifndef FILTER_HELPER_HPP
#define FILTER_HELPER_HPP

#include <vector>

#include <cstdint>
typedef uint32_t u32;

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
void convert32To24(u32* src,u8* dst,unsigned int width, unsigned int height);
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
inline u32 * GetVerticalOffset(u32 * image_pointer,unsigned int width,unsigned int vertical_offset,unsigned int scale=1);

/**
 * A simple struct to keep track of an image pointer, and the image's size.
 */
struct raw_image
{
    u32* image;
    unsigned int width;
    unsigned int height;
};

/**
 * Split an image into multiple sections, with each section being a certain number of rows of the original image
 *
 * NOTE:  The returned images are not separate.  The pointers merely point to different parts of the original image.
 *
 * \param[in] inImage   The original image
 * \param[in] number    The number of sections to split the image into
 * \return              A vector containing the sections of the original image
 */
std::vector<raw_image> splitImage(raw_image inImage, unsigned int number);

#endif //FILTER_HELPER_HPP
