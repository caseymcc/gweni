#include "imglib/jpeg.h"

#include <jpeglib.h>

#include <vector>
#include <cassert>

namespace imglib
{

J_COLOR_SPACE getJpegColorType(Format format)
{
    J_COLOR_SPACE color_type=JCS_UNKNOWN;

    switch(format)
    {
    case Format::GreyScale:
        color_type=J_COLOR_SPACE::JCS_GRAYSCALE;
        break;
    case Format::RGB:
        color_type=J_COLOR_SPACE::JCS_RGB;
        break;
    case Format::YUV:
        color_type=J_COLOR_SPACE::JCS_YCbCr;
        break;
    }

    return color_type;
}

Format getFormat(J_COLOR_SPACE color_type)
{
    Format format=Format::Unknown;

    switch(color_type)
    {
    case J_COLOR_SPACE::JCS_GRAYSCALE:
        format=Format::GreyScale;
        break;
    case J_COLOR_SPACE::JCS_RGB:
        format=Format::RGB;
        break;
    case J_COLOR_SPACE::JCS_YCbCr:
        format=Format::YUV;
        break;
    }

    return format;
}

bool loadJpeg(ImageWrapper image, const char *filename)
{
    FILE *file=fopen(filename, "rb");

    if(!file)
        return false;

    jpeg_decompress_struct jpegDecompress;
    jpeg_error_mgr jerr;

    jpegDecompress.err = jpeg_std_error(&jerr);
    
    jpeg_create_decompress(&jpegDecompress);
    jpeg_stdio_src(&jpegDecompress, file);
    
    jpeg_read_header(&jpegDecompress, TRUE);
    jpeg_start_decompress(&jpegDecompress);
  
    Format format=getFormat(jpegDecompress.out_color_space);
    Depth depth=Depth::Bit8;

    if(!image.resize(format, depth, jpegDecompress.output_width, jpegDecompress.output_height))
        return false;
        
    size_t row_stride=image.stride()*sizeOfPixel(format, depth);
    uint8_t *bufferLinePos=image.data();

    while (jpegDecompress.output_scanline < jpegDecompress.output_height)
    {
        jpeg_read_scanlines(&jpegDecompress, (JSAMPARRAY)&bufferLinePos, 1);
        bufferLinePos+=row_stride;
    }

    jpeg_finish_decompress(&jpegDecompress);
    jpeg_destroy_decompress(&jpegDecompress);

    fclose(file);
    return true;
}

bool saveJpeg(ImageWrapper image, const char *filename)
{
    int y;

    FILE *file=fopen(filename, "wb");
    
    if(!file)
        return false;

    struct jpeg_compress_struct jpegCompress;
    struct jpeg_error_mgr jerr;

    jpegCompress.err=jpeg_std_error(&jerr);

    jpeg_create_compress(&jpegCompress);
    jpeg_stdio_dest(&jpegCompress, file);

    jpegCompress.image_width=image.width();	
    jpegCompress.image_height=image.height();
    jpegCompress.input_components=sizeOfPixel(image.format(), image.depth());
    jpegCompress.in_color_space=getJpegColorType(image.format());

    jpeg_set_defaults(&jpegCompress);

    jpeg_start_compress(&jpegCompress, TRUE);

    size_t row_stride=image.stride()*sizeOfPixel(image.format(), image.depth());
    uint8_t *bufferLinePos=image.data();

    while(jpegCompress.next_scanline < jpegCompress.image_height)
    {
        jpeg_write_scanlines(&jpegCompress, (JSAMPARRAY)&bufferLinePos, 1);
        bufferLinePos+=row_stride;
    }

    jpeg_finish_compress(&jpegCompress);
    jpeg_destroy_compress(&jpegCompress);

    fclose(file);
    return false;
}

}//namespace cvlib