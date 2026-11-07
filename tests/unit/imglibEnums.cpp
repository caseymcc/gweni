/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  imglib's Format and Depth lookups: every enumerator's name reads back as
 *  itself, its channel and bit counts are the ones imglib has always given, and
 *  bitsToDepth resolves the depths that share a bit count (Unknown and Packed)
 *  in declaration order. The lookups are exhaustive switches, so -Werror=switch
 *  stops a new enumerator at compile time; this pins what they return.
 */
#include <imglib/image.h>

#include <doctest/doctest.h>

#include <cstring>

using imglib::Depth;
using imglib::Format;

namespace
{

struct FormatRow
{
    Format format;
    const char *name;
    size_t channels;
};

//every Format, in declaration order
const FormatRow formatRows[]=
{
    {Format::Unknown, "Unknown", 0},
    {Format::Binary, "Binary", 1},
    {Format::GreyScale, "GreyScale", 1},
    {Format::RA, "RA", 2},
    {Format::RGB, "RGB", 3},
    {Format::RGBA, "RGBA", 4},
    {Format::BGRA, "BGRA", 4},
    {Format::YUV, "YUV", 3},
};

struct DepthRow
{
    Depth depth;
    const char *name;
    size_t bits;
};

//every Depth, in declaration order
const DepthRow depthRows[]=
{
    {Depth::Unknown, "Unknown", 0},
    {Depth::Bit1, "1 bit", 1},
    {Depth::Bit8, "8 bit", 8},
    {Depth::Bit10, "10 bit", 10},
    {Depth::Bit12, "12 bit", 12},
    {Depth::Bit14, "14 bit", 14},
    {Depth::Bit16, "16 bit", 16},
    {Depth::Float, "Float", 32},
    {Depth::Double, "Double", 64},
    {Depth::Packed, "Packed", 0},
};

} // namespace

TEST_CASE("imglib: every Format has its name and channel count, and reads back from its name")
{
    for(const FormatRow &row:formatRows)
    {
        CAPTURE(row.name);
        CHECK(std::strcmp(imglib::formatToString(row.format), row.name)==0);
        CHECK(imglib::formatFromString(imglib::formatToString(row.format))==row.format);
        CHECK(imglib::formatToChannels(row.format)==row.channels);
    }

    //the channel counts are the traits' own
    CHECK(imglib::formatToChannels(Format::Binary)==imglib::traits::channels<Format::Binary>());
    CHECK(imglib::formatToChannels(Format::GreyScale)==imglib::traits::channels<Format::GreyScale>());
    CHECK(imglib::formatToChannels(Format::RA)==imglib::traits::channels<Format::RA>());
    CHECK(imglib::formatToChannels(Format::RGB)==imglib::traits::channels<Format::RGB>());
    CHECK(imglib::formatToChannels(Format::RGBA)==imglib::traits::channels<Format::RGBA>());
    CHECK(imglib::formatToChannels(Format::BGRA)==imglib::traits::channels<Format::BGRA>());
    CHECK(imglib::formatToChannels(Format::YUV)==imglib::traits::channels<Format::YUV>());
}

TEST_CASE("imglib: every Depth has its name and bit count, and reads back from its name and bits")
{
    for(const DepthRow &row:depthRows)
    {
        CAPTURE(row.name);
        CHECK(std::strcmp(imglib::depthToString(row.depth), row.name)==0);
        CHECK(imglib::depthFromString(imglib::depthToString(row.depth))==row.depth);
        CHECK(imglib::depthToBits(row.depth)==row.bits);

        //Packed's 0 bits are Unknown's, which is declared first
        if(row.depth!=Depth::Packed)
            CHECK(imglib::bitsToDepth(imglib::depthToBits(row.depth))==row.depth);
    }

    CHECK(imglib::bitsToDepth(0)==Depth::Unknown);
}

TEST_CASE("imglib: unknown names, bit counts and out-of-range values keep their fallbacks")
{
    CHECK(imglib::formatFromString("NoSuchFormat")==Format::Binary);
    CHECK(imglib::depthFromString("NoSuchDepth")==Depth::Bit1);
    CHECK(imglib::bitsToDepth(7)==Depth::Unknown);

    //one past the last enumerator of each
    const Format pastFormat=static_cast<Format>(static_cast<int>(Format::YUV)+1);
    const Depth pastDepth=static_cast<Depth>(static_cast<int>(Depth::Packed)+1);

    CHECK(std::strcmp(imglib::formatToString(pastFormat), "Unknown")==0);
    CHECK(imglib::formatToChannels(pastFormat)==0);
    CHECK(std::strcmp(imglib::depthToString(pastDepth), "Unknown")==0);
    CHECK(imglib::depthToBits(pastDepth)==0);
}
