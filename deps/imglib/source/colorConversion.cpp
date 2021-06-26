#include "imglib/colorConversion.h"

#include <cassert>

namespace imglib
{

bool rgbToRGBA(uint8_t *srcBuffer, size_t srcSize, Depth srcDepth, size_t srcWidth, size_t srcHeight,
    uint8_t *dstBuffer, size_t dstSize, Depth dstDepth, size_t dstWidth, size_t dstHeight)
{
    //not implemented
    assert(false);
    return false;
}

bool rgbToGreyScale(uint8_t *srcBuffer, size_t srcSize, Depth srcDepth, size_t srcWidth, size_t srcHeight, 
    uint8_t *dstBuffer, size_t dstSize, Depth dstDepth, size_t dstWidth, size_t dstHeight)
{
    if(srcDepth != dstDepth)
        return false;

    size_t count=srcWidth*srcHeight;

    if(count > dstWidth*dstHeight)
        return false;

    float r, g, b;
    uint8_t *src=srcBuffer;
    uint8_t *dst=dstBuffer;

    for(size_t i=0; i<count; ++i)
    {
        r=(float)src[0];
        g=(float)src[1];
        b=(float)src[2];

        (*dst)=(uint8_t)((r*0.2126f)+(g*0.7152f)+(b*0.0722f));
        src+=3;
        dst++;
    }
    
    return true;
}

}//namespace imglib
