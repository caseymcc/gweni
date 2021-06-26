#ifndef _imglib_colorConversion_h_
#define _imglib_colorConversion_h_

#include "imglib_export.h"
#include "imglib/image.h"

#include <cstdint>

namespace imglib
{

template<typename _Image>
bool convert(_Image &srcImage, _Image &dstImage);

template<typename _Image>
bool rgbToRGBA(_Image &srcImage, _Image &dstImage);

IMGLIB_EXPORT bool rgbToRGBA(uint8_t *srcBuffer, size_t srcSize, Depth srcDepth, size_t srcWidth, size_t srcHeight, 
    uint8_t *dstBuffer, size_t dstSize, Depth dstDepth, size_t dstWidth, size_t dstHeight);

template<typename _Image>
bool rgbToGreyScale(_Image &srcImage, _Image &dstImage);

IMGLIB_EXPORT bool rgbToGreyScale(uint8_t *srcBuffer, size_t srcSize, Depth srcDepth, size_t srcWidth, size_t srcHeight, 
    uint8_t *dstBuffer, size_t dstSize, Depth dstDepth, size_t dstWidth, size_t dstHeight);

}//namespace imglib

//source
#include "colorConversion.inl"

#endif //_imglib_colorConversion_h_