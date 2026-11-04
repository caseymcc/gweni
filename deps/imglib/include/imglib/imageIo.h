#ifndef _imglib_imageIo_h_
#define _imglib_imageIo_h_

#include "imglib_filesystem.h"

#include "imglib/ppm.h"
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

template<typename _ImageType>
bool loadBuffer(_ImageType &image, const uint8_t *buffer, size_t size, std::string &format)
{
    if(format=="png")
        return loadPngBuffer(image, buffer, size);

    assert(false); //not supported yet
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
