#ifndef _imglib_png_h_
#define _imglib_png_h

#include "imglib_export.h"
#include "imglib/image.h"

namespace imglib
{

//On false (not a PNG, unsupported, a truncated file or a buffer shorter than the PNG it holds) the
//image's contents are undefined: it may already have been resized and hold some of the rows
IMGLIB_EXPORT bool loadPng(ImageWrapper image, const char *filename);
IMGLIB_EXPORT bool loadPngBuffer(ImageWrapper image, const uint8_t *buffer, size_t bufferSize);

IMGLIB_EXPORT bool savePng(ImageWrapper image, const char *filename);

}//namespace imglib

#endif //_imglib_png_h