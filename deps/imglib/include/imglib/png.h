#ifndef _imglib_png_h_
#define _imglib_png_h_

#include "imglib_export.h"
#include "imglib/image.h"

namespace imglib
{

//On false (not a PNG, unsupported, a truncated file or a buffer shorter than the PNG it holds) the
//image's contents are undefined: it may already have been resized and hold some of the rows
IMGLIB_EXPORT bool loadPng(ImageWrapper image, const char *filename);
//The buffer loader prints nothing (libpng's errors and warnings are dropped), and fails before any
//pixel buffer is allocated for an image of more than maxPixels pixels (0: no limit)
IMGLIB_EXPORT bool loadPngBuffer(ImageWrapper image, const uint8_t *buffer, size_t bufferSize, size_t maxPixels=0);

IMGLIB_EXPORT bool savePng(ImageWrapper image, const char *filename);

}//namespace imglib

#endif //_imglib_png_h_