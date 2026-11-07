#include "imglib/image.h"

#include <cstring>

namespace imglib
{

//every Format is listed so -Werror=switch catches a newly added one instead of it silently
//having no name; nullptr past the last Format ends formatFromString's walk
static const char *formatName(Format format)
{
    switch(format)
    {
    case Format::Unknown:
        return "Unknown";
    case Format::Binary:
        return "Binary";
    case Format::GreyScale:
        return "GreyScale";
    case Format::RA:
        return "RA";
    case Format::RGB:
        return "RGB";
    case Format::RGBA:
        return "RGBA";
    case Format::BGRA:
        return "BGRA";
    case Format::YUV:
        return "YUV";
    }
    return nullptr;
}

const char *formatToString(Format format)
{
    const char *name=formatName(format);

    if(name==nullptr)
        return "Unknown";
    return name;
}

Format formatFromString(const char *name)
{
    //walks the Formats in declaration order through formatName, so there is no list to keep in step
    for(int value=0; ; ++value)
    {
        Format format=static_cast<Format>(value);
        const char *formatString=formatName(format);

        if(formatString==nullptr)
            break;
        if(strcmp(formatString, name)==0)
            return format;
    }
    return Format::Binary;
}

size_t formatToChannels(Format format)
{
    //every Format is listed so -Werror=switch catches a newly added one instead of it silently
    //having 0 channels
    switch(format)
    {
    case Format::Unknown:
        return 0;
    case Format::Binary:
        return traits::channels<Format::Binary>();
    case Format::GreyScale:
        return traits::channels<Format::GreyScale>();
    case Format::RA:
        return traits::channels<Format::RA>();
    case Format::RGB:
        return traits::channels<Format::RGB>();
    case Format::RGBA:
        return traits::channels<Format::RGBA>();
    case Format::BGRA:
        return traits::channels<Format::BGRA>();
    case Format::YUV:
        return traits::channels<Format::YUV>();
    }
    //unreachable for any valid Format, kept for -Wreturn-type
    return 0;
}

Format channelsToFormat(size_t channels)
{
    switch(channels)
    {
    case 1:
        return Format::GreyScale;
        break;
    case 2:
        return Format::RA;
        break;
    case 3:
        return Format::RGB;
        break;
    case 4:
        return Format::RGBA;
        break;
    }
    return Format::Unknown;
}

//every Depth is listed so -Werror=switch catches a newly added one instead of it silently
//having no name; nullptr past the last Depth ends the depthFromString and bitsToDepth walks
static const char *depthName(Depth depth)
{
    switch(depth)
    {
    case Depth::Unknown:
        return "Unknown";
    case Depth::Bit1:
        return "1 bit";
    case Depth::Bit8:
        return "8 bit";
    case Depth::Bit10:
        return "10 bit";
    case Depth::Bit12:
        return "12 bit";
    case Depth::Bit14:
        return "14 bit";
    case Depth::Bit16:
        return "16 bit";
    case Depth::Float:
        return "Float";
    case Depth::Double:
        return "Double";
    case Depth::Packed:
        return "Packed";
    }
    return nullptr;
}

const char *depthToString(Depth depth)
{
    const char *name=depthName(depth);

    if(name==nullptr)
        return "Unknown";
    return name;
}

Depth depthFromString(const char *name)
{
    //walks the Depths in declaration order through depthName, so there is no list to keep in step
    for(int value=0; ; ++value)
    {
        Depth depth=static_cast<Depth>(value);
        const char *depthString=depthName(depth);

        if(depthString==nullptr)
            break;
        if(strcmp(depthString, name)==0)
            return depth;
    }
    return Depth::Bit1;
}

size_t depthToBits(Depth depth)
{
    //every Depth is listed so -Werror=switch catches a newly added one instead of it silently
    //having 0 bits
    switch(depth)
    {
    case Depth::Unknown:
        return 0;
    case Depth::Bit1:
        return 1;
    case Depth::Bit8:
        return 8;
    case Depth::Bit10:
        return 10;
    case Depth::Bit12:
        return 12;
    case Depth::Bit14:
        return 14;
    case Depth::Bit16:
        return 16;
    case Depth::Float:
        return 32;
    case Depth::Double:
        return 64;
    case Depth::Packed:
        //depends on the pack format, which Depth does not carry
        return 0;
    }
    //unreachable for any valid Depth, kept for -Wreturn-type
    return 0;
}

Depth bitsToDepth(size_t bits)
{
    //walks the Depths in declaration order, so of the depths with equal bits (Unknown and Packed,
    //both 0) the first declared is returned
    for(int value=0; depthName(static_cast<Depth>(value))!=nullptr; ++value)
    {
        Depth depth=static_cast<Depth>(value);

        if(depthToBits(depth)==bits)
            return depth;
    }
    return Depth::Unknown;
}

template<Format _Format>
size_t sizeOfImageDepth(Depth depth)
{
    //every Depth is listed so -Werror=switch catches a newly added one instead of it
    //silently returning zero
    switch(depth)
    {
    case Depth::Unknown:
        return 0;
    case Depth::Packed:
        //several pack formats are expected, none of them has a fixed pixel size yet
        return 0;
    case Depth::Bit1:
        return traits::sizeOf<_Format, Depth::Bit1>();
    case Depth::Bit8:
        return traits::sizeOf<_Format, Depth::Bit8>();
    case Depth::Bit10:
        return traits::sizeOf<_Format, Depth::Bit10>();
    case Depth::Bit12:
        return traits::sizeOf<_Format, Depth::Bit12>();
    case Depth::Bit14:
        return traits::sizeOf<_Format, Depth::Bit14>();
    case Depth::Bit16:
        return traits::sizeOf<_Format, Depth::Bit16>();
    case Depth::Float:
        return traits::sizeOf<_Format, Depth::Float>();
    case Depth::Double:
        return traits::sizeOf<_Format, Depth::Double>();
    }
    return 0;
}

size_t sizeOfPixel(Format format, Depth depth)
{
    //every Format is listed so -Wswitch catches a newly added one instead of it silently
    //returning zero, which made libjpeg abort on a BGRA image
    switch(format)
    {
    case Format::Unknown:
        return 0;
    case Format::Binary:
        return sizeOfImageDepth<Format::Binary>(depth);
    case Format::GreyScale:
        return sizeOfImageDepth<Format::GreyScale>(depth);
    case Format::RA:
        return sizeOfImageDepth<Format::RA>(depth);
    case Format::RGB:
        return sizeOfImageDepth<Format::RGB>(depth);
    case Format::RGBA:
        return sizeOfImageDepth<Format::RGBA>(depth);
    case Format::BGRA:
        return sizeOfImageDepth<Format::BGRA>(depth);
    case Format::YUV:
        return sizeOfImageDepth<Format::YUV>(depth);
    }
    //unreachable for any valid Format, kept for -Wreturn-type
    return 0;
}


size_t sizeOfImage(Format format, Depth depth, size_t width, size_t height)
{
	return sizeOfPixel(format, depth)*width*height;
}

}//namespace imglib
