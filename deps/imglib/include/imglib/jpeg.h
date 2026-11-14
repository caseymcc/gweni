#ifndef _imglib_jpeg_h_
#define _imglib_jpeg_h_

#include "imglib_export.h"
#include "imglib/image.h"

namespace imglib
{

IMGLIB_EXPORT bool loadJpeg(ImageWrapper image, const char *filename);
//Decodes a JPEG held in memory, at 1/scaleDenom of its size where scaleDenom is 2, 4 or 8 (libjpeg's
//reduced decode, each side rounded up; any other value decodes it whole), so the full-size image is
//never allocated. False, with the image's contents undefined, for a buffer that is no JPEG, a
//corrupt header or one libjpeg rejects, a buffer that ends before the image does (libjpeg's
//premature-end warning), a colour space with no Format (CMYK, YCCK), or an image of more than
//maxPixels pixels as it would be decoded (0: no limit), which is found before any pixel buffer is
//allocated. Nothing is written to stderr.
IMGLIB_EXPORT bool loadJpegBuffer(ImageWrapper image, const uint8_t *buffer, size_t bufferSize,
    unsigned int scaleDenom=1, size_t maxPixels=0);
IMGLIB_EXPORT bool saveJpeg(ImageWrapper image, const char *filename);

}//namespace imglib

#endif //_imglib_jpeg_h_