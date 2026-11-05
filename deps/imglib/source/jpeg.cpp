#include "imglib/jpeg.h"

#include <jpeglib.h>

#include <vector>
#include <cassert>
#include <csetjmp>
#include <cstdio>

namespace imglib
{

//libjpeg's default error_exit calls exit(), which would kill the process on a bad file or a
//rejected parameter; this manager longjmps back to the caller's setjmp so the failure is reported
struct JpegErrorManager
{
    jpeg_error_mgr pub;
    jmp_buf jmpBuffer;
};

void jpegErrorExit(j_common_ptr info)
{
    JpegErrorManager *error=(JpegErrorManager *)info->err;

    longjmp(error->jmpBuffer, 1);
}

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
    default:
        //RGBA, BGRA, RA and Binary have no jpeg color space, JCS_UNKNOWN reports that
        color_type=JCS_UNKNOWN;
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
    JpegErrorManager jerr;
    //volatile so its value survives a longjmp back into this frame
    volatile bool success=false;

    jpegDecompress.err=jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit=jpegErrorExit;

    jpeg_create_decompress(&jpegDecompress);

    if(setjmp(jerr.jmpBuffer)==0)
    {
        jpeg_stdio_src(&jpegDecompress, file);
        jpeg_read_header(&jpegDecompress, TRUE);

        //out_color_space is what libjpeg writes into the buffer; JCS_UNKNOWN, CMYK and YCCK have
        //no Format, so there is no correctly sized buffer to decode them into
        Format format=getFormat(jpegDecompress.out_color_space);
        Depth depth=Depth::Bit8;

        if(format!=Format::Unknown)
        {
            jpeg_start_decompress(&jpegDecompress);

            if(image.resize(format, depth, jpegDecompress.output_width, jpegDecompress.output_height))
            {
                size_t row_stride=image.stride()*sizeOfPixel(format, depth);
                uint8_t *bufferLinePos=image.data();

                while(jpegDecompress.output_scanline < jpegDecompress.output_height)
                {
                    jpeg_read_scanlines(&jpegDecompress, (JSAMPARRAY)&bufferLinePos, 1);
                    bufferLinePos+=row_stride;
                }

                jpeg_finish_decompress(&jpegDecompress);
                success=true;
            }
        }
    }

    //destroy releases the decompressor whether it was started, aborted or longjmped out of
    jpeg_destroy_decompress(&jpegDecompress);

    fclose(file);
    return success;
}

bool saveJpeg(ImageWrapper image, const char *filename)
{
    J_COLOR_SPACE color_type=getJpegColorType(image.format());

    //rejected before the file is created, so an unsupported image leaves nothing behind. the
    //alpha formats are rejected rather than written without their alpha channel
    if(color_type==JCS_UNKNOWN)
        return false;

    //libjpeg takes one byte per sample here, so the wider and floating point depths have no mapping
    if(image.depth()!=Depth::Bit8)
        return false;

    struct jpeg_compress_struct jpegCompress;
    JpegErrorManager jerr;
    //volatile so its value survives a longjmp back into this frame
    volatile bool success=false;

    FILE *file=fopen(filename, "wb");

    if(!file)
        return false;

    jpegCompress.err=jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit=jpegErrorExit;

    jpeg_create_compress(&jpegCompress);

    if(setjmp(jerr.jmpBuffer)==0)
    {
        jpeg_stdio_dest(&jpegCompress, file);

        jpegCompress.image_width=image.width();
        jpegCompress.image_height=image.height();
        jpegCompress.input_components=(int)formatToChannels(image.format());
        jpegCompress.in_color_space=color_type;

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
        success=true;
    }

    jpeg_destroy_compress(&jpegCompress);

    fclose(file);
    return success;
}

}//namespace cvlib