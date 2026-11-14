#include "imglib/jpeg.h"

#include <jpeglib.h>
#include <jerror.h>

#include <vector>
#include <cstdint>
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

    //every Format is listed so -Werror=switch catches a newly added one instead of it silently
    //becoming JCS_UNKNOWN; JCS_UNKNOWN reports a format that has no jpeg color space
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
    case Format::RGBA:
        //writable through libjpeg-turbo's JCS_EXT_RGBA (4 input components)
        color_type=JCS_UNKNOWN;
        break;
    case Format::BGRA:
        //writable through libjpeg-turbo's JCS_EXT_BGRA (4 input components)
        color_type=JCS_UNKNOWN;
        break;
    case Format::Unknown:
    case Format::Binary:
    case Format::RA:
        //no meaningful jpeg color space
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
    default:
        //J_COLOR_SPACE is libjpeg's, not ours, so it gets a default rather than the
        //exhaustive treatment imglib's own enums get; CMYK/YCCK/JCS_EXT_* have no Format
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

    //setjmp before create: jpeg_create_decompress can itself raise JERR_BAD_LIB_VERSION,
    //JERR_BAD_STRUCT_SIZE or an out-of-memory error, which error_exit turns into a longjmp
    if(setjmp(jerr.jmpBuffer)==0)
    {
        jpeg_create_decompress(&jpegDecompress);

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

//The buffer loader's error manager: error_exit longjmps as JpegErrorManager's does, nothing is
//printed, and the premature-end warning (libjpeg's fill for data that stops early) is recorded,
//so a truncated buffer fails instead of decoding as grey rows
struct JpegBufferErrorManager
{
    JpegErrorManager base;
    bool truncated;
};

void jpegSilentOutput(j_common_ptr)
{}

void jpegBufferEmit(j_common_ptr info, int level)
{
    JpegBufferErrorManager *error=(JpegBufferErrorManager *)info->err;

    if(level<0)
    {
        if(info->err->msg_code==JWRN_JPEG_EOF)
            error->truncated=true;
        info->err->num_warnings++;
    }
}

bool loadJpegBuffer(ImageWrapper image, const uint8_t *buffer, size_t bufferSize, unsigned int scaleDenom,
    size_t maxPixels)
{
    //SOI and the start of a marker; libjpeg would also reject a shorter or other buffer, but with an error
    if((buffer==nullptr)||(bufferSize<3)||(buffer[0]!=0xff)||(buffer[1]!=0xd8)||(buffer[2]!=0xff))
        return false;

    jpeg_decompress_struct jpegDecompress;
    JpegBufferErrorManager jerr;
    //volatile so their values survive a longjmp back into this frame
    volatile bool success=false;
    volatile bool created=false;

    jpegDecompress.err=jpeg_std_error(&jerr.base.pub);
    jerr.base.pub.error_exit=jpegErrorExit;
    jerr.base.pub.output_message=jpegSilentOutput;
    jerr.base.pub.emit_message=jpegBufferEmit;
    jerr.truncated=false;

    if(setjmp(jerr.base.jmpBuffer)==0)
    {
        jpeg_create_decompress(&jpegDecompress);
        created=true;

        jpeg_mem_src(&jpegDecompress, buffer, (unsigned long)bufferSize);
        jpeg_read_header(&jpegDecompress, TRUE);

        if((scaleDenom==2)||(scaleDenom==4)||(scaleDenom==8))
        {
            jpegDecompress.scale_num=1;
            jpegDecompress.scale_denom=scaleDenom;
        }
        jpeg_calc_output_dimensions(&jpegDecompress);

        Format format=getFormat(jpegDecompress.out_color_space);
        Depth depth=Depth::Bit8;
        const uint64_t pixels=(uint64_t)jpegDecompress.output_width*jpegDecompress.output_height;

        if((format!=Format::Unknown)&&((maxPixels==0)||(pixels<=maxPixels))&&!jerr.truncated)
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
                success=!jerr.truncated;
            }
        }
    }

    if(created)
        jpeg_destroy_decompress(&jpegDecompress);
    return success;
}

bool saveJpeg(ImageWrapper image, const char *filename)
{
    J_COLOR_SPACE color_type=getJpegColorType(image.format());

    //rejected before the file is created; a failure after that point removes the destination on
    //the way out, so a false return always means no file was left behind. the alpha formats are
    //rejected rather than written without their alpha channel
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

    //setjmp before create: jpeg_create_compress can itself raise JERR_BAD_LIB_VERSION,
    //JERR_BAD_STRUCT_SIZE or an out-of-memory error, which error_exit turns into a longjmp
    if(setjmp(jerr.jmpBuffer)==0)
    {
        jpeg_create_compress(&jpegCompress);

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

    //the destination was truncated by fopen before anything could fail, so there is nothing to
    //preserve: drop the empty or partial file rather than leave it looking like an image
    if(!success)
        remove(filename);

    return success;
}

}//namespace cvlib