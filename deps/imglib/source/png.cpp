#include "imglib/png.h"
#include <png.h>

#include <vector>
#include <cstring>
#include <cstdio>

namespace imglib
{

//no png color type or bit depth has these values, so they report an unsupported format/depth
const png_byte pngColorTypeUnsupported=0xff;
const png_byte pngBitDepthUnsupported=0;

png_byte getPngColorType(Format format)
{
    png_byte color_type;

    //every Format is listed so -Werror=switch catches a newly added one instead of it silently
    //becoming unsupported
    switch(format)
    {
    case Format::GreyScale:
        color_type=PNG_COLOR_TYPE_GRAY;
        break;
    case Format::RA:
        color_type=PNG_COLOR_TYPE_GRAY_ALPHA;
        break;
    case Format::RGB:
        color_type=PNG_COLOR_TYPE_RGB;
        break;
    case Format::RGBA:
        color_type=PNG_COLOR_TYPE_RGB_ALPHA;
        break;
    case Format::BGRA:
        //writable as PNG_COLOR_TYPE_RGB_ALPHA with png_set_bgr
        color_type=pngColorTypeUnsupported;
        break;
    case Format::Unknown:
    case Format::Binary:
    case Format::YUV:
        //no meaningful png color type
        color_type=pngColorTypeUnsupported;
        break;
    }

    return color_type;
}

png_byte getPngBitDepth(Depth depth)
{
    png_byte bit_depth;

    //libpng only accepts 1, 2, 4, 8 and 16 bits per sample. Every Depth is listed so
    //-Werror=switch catches a newly added one instead of it silently becoming unsupported
    switch(depth)
    {
    case Depth::Bit8:
        bit_depth=8;
        break;
    case Depth::Bit16:
        bit_depth=16;
        break;
    case Depth::Bit1:
        //writable as 1 with png_set_packing: imglib stores Bit1 as 1 byte per sample, png
        //wants packed bits
        bit_depth=pngBitDepthUnsupported;
        break;
    case Depth::Bit10:
        //writable as 16 with png_set_sBIT + png_set_shift, already stored in uint16_t
        bit_depth=pngBitDepthUnsupported;
        break;
    case Depth::Bit12:
        //writable as 16 with png_set_sBIT + png_set_shift, already stored in uint16_t
        bit_depth=pngBitDepthUnsupported;
        break;
    case Depth::Bit14:
        //writable as 16 with png_set_sBIT + png_set_shift, already stored in uint16_t
        bit_depth=pngBitDepthUnsupported;
        break;
    case Depth::Float:
        //libpng has no floating point samples, imglib would have to quantise to 16 bit
        bit_depth=pngBitDepthUnsupported;
        break;
    case Depth::Double:
        //libpng has no floating point samples, imglib would have to quantise to 16 bit
        bit_depth=pngBitDepthUnsupported;
        break;
    case Depth::Packed:
        //no meaningful png bit depth
        bit_depth=pngBitDepthUnsupported;
        break;
    case Depth::Unknown:
        //no meaningful png bit depth
        bit_depth=pngBitDepthUnsupported;
        break;
    }

    return bit_depth;
}

bool isLittleEndianHost()
{
    const uint16_t one=1;
    uint8_t firstByte;

    memcpy(&firstByte, &one, 1);
    return firstByte==1;
}

std::vector<png_bytep> getRowPointers(ImageWrapper &image)
{
    std::vector<png_bytep> row_pointers(image.height());
    png_byte *imageData=(png_byte *)image.data();

    size_t pos=0;
    size_t stride=image.width()*sizeOfPixel(image.format(), image.depth());

    for(size_t y=0; y<image.height(); y++)
    {
        row_pointers[y]=&imageData[pos];
        pos+=stride;
    }

    return row_pointers;
}

Format getFormatFromPngColorType(png_byte color_type)
{
    switch(color_type)
    {
    case PNG_COLOR_TYPE_GRAY:
        return Format::GreyScale;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        return Format::RA;
    case PNG_COLOR_TYPE_RGB:
        return Format::RGB;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        return Format::RGBA;
    default:
        return Format::Unknown;
    }
}

//the depth policy of both loaders: every PNG is read as 8 bits per sample, which is what the
//renderer uploads. The transforms expand a palette to RGB, grey of 1, 2 or 4 bits to 8 and a tRNS
//chunk to an alpha channel, and scale 16 bits to 8 (rounded), so every valid file comes out as 8 bit
//GRAY, GA, RGB or RGBA. Sizes the image for those rows; false (before any row is read) if the rows
//libpng will write do not fit it
bool setupPngRead(png_structp png, png_infop info, ImageWrapper &image)
{
    png_set_expand(png);
    png_set_scale_16(png);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);

    //read after the update: the transforms change the colour type and depth the rows come out in
    png_byte color_type=png_get_color_type(png, info);
    png_byte bit_depth=png_get_bit_depth(png, info);
    Format format=getFormatFromPngColorType(color_type);

    if((bit_depth!=8)||(format==Format::Unknown))
        return false;

    size_t width=png_get_image_width(png, info);
    size_t height=png_get_image_height(png, info);

    //the stride getRowPointers gives each row
    if(png_get_rowbytes(png, info)!=width*sizeOfPixel(format, Depth::Bit8))
        return false;

    return image.resize(format, Depth::Bit8, width, height);
}

bool loadPng(ImageWrapper image, const char *filename)
{
    FILE *file=fopen(filename, "rb");

    if(!file)
        return false;

    png_structp png_ptr=NULL;
    png_infop info_ptr=NULL;
    //row_pointers is declared before the setjmp so a longjmp out of libpng cannot skip its destructor
    std::vector<png_bytep> row_pointers;
    //volatile so its value survives a longjmp back into this frame
    volatile bool success=false;
    char header[8];

    if(fread(header, 1, 8, file)==8)
    {
        if(!png_sig_cmp((png_const_bytep)header, 0, 8))
            png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    }

    if(png_ptr)
        info_ptr=png_create_info_struct(png_ptr);

    if(info_ptr)
    {
        if(setjmp(png_jmpbuf(png_ptr))==0)
        {
            png_init_io(png_ptr, file);
            png_set_sig_bytes(png_ptr, 8);
            png_read_info(png_ptr, info_ptr);

            if(setupPngRead(png_ptr, info_ptr, image))
            {
                row_pointers=getRowPointers(image);

                png_read_image(png_ptr, row_pointers.data());
                success=true;
            }
        }
    }

    if(png_ptr)
        png_destroy_read_struct(&png_ptr, info_ptr?&info_ptr:NULL, NULL);

    fclose(file);
    return success;
}


struct BufferInfo
{
    BufferInfo(const uint8_t *buffer, size_t size, size_t index):buffer(buffer), index(index), size(size) {}

    const uint8_t *buffer;
    size_t index;
    size_t size;
};

void readBuffer(png_structp png_ptr, png_bytep data, png_size_t length)
{
    BufferInfo *bufferInfo=(BufferInfo *)png_get_io_ptr(png_ptr);

    //a subtraction, so it cannot overflow: loadPngBuffer starts index at 8 only for a buffer of 8
    //bytes or more. png_error does not return, it longjmps to loadPngBuffer's setjmp, which returns
    //false (this frame holds nothing with a destructor)
    if(length>bufferInfo->size-bufferInfo->index)
        png_error(png_ptr, "read past end of buffer");

    memcpy(data, bufferInfo->buffer+bufferInfo->index, length);
    bufferInfo->index+=length;
}

bool loadPngBuffer(ImageWrapper image, const uint8_t *buffer, size_t bufferSize)
{
    //png_sig_cmp reads 8 bytes whatever the size
    if((buffer==nullptr)||(bufferSize<8))
        return false;

    if(png_sig_cmp((png_const_bytep)buffer, 0, 8))
        return false;

    png_infop info_ptr=NULL;
    //row_pointers is declared before the setjmp so a longjmp out of libpng cannot skip its destructor
    std::vector<png_bytep> row_pointers;
    //volatile so its value survives a longjmp back into this frame
    volatile bool success=false;

    BufferInfo bufferInfo(buffer, bufferSize, 8);

    png_structp png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if(png_ptr)
        info_ptr=png_create_info_struct(png_ptr);

    if(info_ptr)
    {
        if(setjmp(png_jmpbuf(png_ptr))==0)
        {
            png_set_read_fn(png_ptr, (png_voidp)&bufferInfo, &readBuffer);
            png_set_sig_bytes(png_ptr, 8);
            png_read_info(png_ptr, info_ptr);

            if(setupPngRead(png_ptr, info_ptr, image))
            {
                row_pointers=getRowPointers(image);

                png_read_image(png_ptr, row_pointers.data());
                success=true;
            }
        }
    }

    if(png_ptr)
        png_destroy_read_struct(&png_ptr, info_ptr?&info_ptr:NULL, NULL);

    return success;
}

bool savePng(ImageWrapper image, const char *filename)
{
    png_byte color_type=getPngColorType(image.format());
    png_byte bit_depth=getPngBitDepth(image.depth());

    //rejected before the file is created; a failure after that point removes the destination on
    //the way out, so a false return always means no file was left behind
    if((color_type==pngColorTypeUnsupported)||(bit_depth==pngBitDepthUnsupported))
        return false;

    FILE *file=fopen(filename, "wb");

    if(!file)
        return false;

    png_infop info=NULL;
    //row_pointers is built before the setjmp so a longjmp out of libpng cannot skip its destructor
    std::vector<png_bytep> row_pointers=getRowPointers(image);
    //volatile so its value survives a longjmp back into this frame
    volatile bool success=false;

    png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if(png)
        info=png_create_info_struct(png);

    if(info)
    {
        if(setjmp(png_jmpbuf(png))==0)
        {
            png_init_io(png, file);

            png_set_IHDR(
                png,
                info,
                image.width(), image.height(),
                bit_depth,
                color_type,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT
            );
            png_write_info(png, info);

            //PNG stores 16 bit samples big-endian and the image holds them in host order; libpng's
            //swap is unconditional, so it is asked for only where the two differ
            if((image.depth()==Depth::Bit16)&&isLittleEndianHost())
                png_set_swap(png);

            // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
            // Use png_set_filler().
            //png_set_filler(png, 0, PNG_FILLER_AFTER);

            png_write_image(png, row_pointers.data());
            png_write_end(png, NULL);
            success=true;
        }
    }

    if(png)
        png_destroy_write_struct(&png, info?&info:NULL);

    fclose(file);

    //the destination was truncated by fopen before anything could fail, so there is nothing to
    //preserve: drop the empty or partial file rather than leave it looking like an image
    if(!success)
        remove(filename);

    return success;
}

}//namespace cvlib