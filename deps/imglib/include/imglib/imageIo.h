#ifndef _imglib_imageIo_h_
#define _imglib_imageIo_h_

#include "imglib_filesystem.h"

#include "imglib/ppm.h"
#include "imglib/probe.h"
#ifdef IMGLIB_PNG
#include "imglib/png.h"
#endif//IMGLIB_PNG
#ifdef IMGLIB_JPG
#include "imglib/jpeg.h"
#endif//IMGLIB_JPG

#include <cassert>

namespace imglib
{

template<typename _ImageType>
bool load(_ImageType &image, const std::string &location)
{
    fs::path path(location);

    if(!path.has_extension())
    {
        assert(false); //can only sort by extension at the moment
        return false;
    }

    std::string ext=path.extension().string();

    if(ext==".ppm")
        return loadPpm(image, location.c_str());
#ifdef IMGLIB_PNG
    else if(ext==".png")
        return loadPng(image, location.c_str());
#endif//IMGLIB_PNG
#ifdef IMGLIB_JPG
    else if((ext==".jpg") || (ext==".jpeg"))
        return loadJpeg(image, location.c_str());
#endif//IMGLIB_JPG
    

    assert(false); //not supported yet
    return false;
}

//How loadBuffer decodes: a JPEG at 1/scaleDenom of its size (2, 4 or 8; a PNG is always decoded
//whole), and no image of more than maxPixels pixels as decoded (0: no limit)
struct BufferLoadOptions
{
    unsigned int scaleDenom=1;
    size_t maxPixels=0;
};

//Decodes an image held in memory, its format found from its first bytes (detectFormat). False for
//bytes of no format it reads and for any failure of that format's loader; it never asserts.
template<typename _ImageType>
bool loadBuffer(_ImageType &image, const uint8_t *buffer, size_t size, const BufferLoadOptions &options=BufferLoadOptions())
{
    switch(detectFormat(buffer, size))
    {
    case FileFormat::Png:
#ifdef IMGLIB_PNG
        return loadPngBuffer(image, buffer, size, options.maxPixels);
#else
        break;
#endif//IMGLIB_PNG
    case FileFormat::Jpeg:
#ifdef IMGLIB_JPG
        return loadJpegBuffer(image, buffer, size, options.scaleDenom, options.maxPixels);
#else
        break;
#endif//IMGLIB_JPG
    case FileFormat::Unknown:
        break;
    }
    return false;
}

template<typename _ImageType>
bool save(_ImageType &image, const std::string &location)
{
    fs::path path(location);

    if(!path.has_extension())
    {
        assert(false); //can only sort by extension at the moment
        return false;
    }

    std::string ext=path.extension().string();

    if(ext==".ppm")
        return savePpm(image, location.c_str());
#ifdef IMGLIB_PNG
    else if(ext==".png")
        return savePng(image, location.c_str());
#endif//IMGLIB_PNG
#ifdef IMGLIB_JPG
    else if((ext==".jpg") || (ext==".jpeg"))
        return saveJpeg(image, location.c_str());
#endif//IMGLIB_JPG

    assert(false); //not supported yet
    return false;
}

}//namespace imglib

#endif //_imglib_imageIo_h_
