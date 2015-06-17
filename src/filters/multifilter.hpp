
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
    ///Output image buffer
    u32* outImage;
public:
    multifilter(std::vector<std::string> filters,unsigned int X,unsigned int, Y);
    getOutX();
    getOutY();
    setInImage(u32* image);
    setOutImage(u32* image);
    run();
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
