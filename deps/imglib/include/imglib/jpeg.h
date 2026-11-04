#ifndef _imglib_jpeg_h_
#define _imglib_jpeg_h_

#include "imglib_export.h"
#include "imglib/image.h"

namespace imglib
{

IMGLIB_EXPORT bool loadJpeg(ImageWrapper image, const char *filename);
IMGLIB_EXPORT bool saveJpeg(ImageWrapper image, const char *filename);

}//namespace imglib

#endif //_imglib_jpeg_h_