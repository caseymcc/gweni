#include <cassert>

namespace imglib
{

template<typename _Image>
bool convert(_Image &srcImage, _Image &dstImage)
{
    if((location(srcImage) != Location::System) || (location(dstImage) != Location::System))
    {
        assert(false);
        return false;
    }

    switch(format(srcImage))
    {
    case Format::RGB:
    {
        switch(format(dstImage))
        {
        case Format::RGBA:
            return rgbToRGBA(srcImage, dstImage);
		case Format::GreyScale:
			return rgbToGreyScale(srcImage, dstImage);
        default:
            assert(false); //not implemented
            return false;
        }
    }
    break;
    default:
        assert(false); //not implemented
        return false;
    }

    return false;

}

template<typename _Image>
bool rgbToRGBA(_Image &srcImage, _Image &dstImage)
{
    return rgbToRGBA(data(srcImage), dataSize(srcImage), depth(srcImage), width(srcImage), height(srcImage),
        data(dstImage), dataSize(dstImage), depth(dstImage), width(dstImage), height(dstImage));
}

template<typename _Image>
bool rgbToGreyScale(_Image &srcImage, _Image &dstImage)
{
    return rgbToGreyScale(data(srcImage), dataSize(srcImage), depth(srcImage), width(srcImage), height(srcImage),
        data(dstImage), dataSize(dstImage), depth(dstImage), width(dstImage), height(dstImage));
}

}//namespace imglib
