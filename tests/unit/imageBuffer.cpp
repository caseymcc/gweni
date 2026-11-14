/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  imglib's in-memory image loading, the path gweni's byte loaders take
 *  (imglib::loadBuffer, loadJpegBuffer, detectFormat and probeImage):
 *  - the format is found from the first bytes: the PNG signature and a JPEG's
 *    FF D8 FF; anything else (a GIF, text, noise, a null or short buffer) is
 *    Unknown and loadBuffer returns false for it, with no assert;
 *  - probeImage reads a PNG's IHDR and a JPEG's frame header for the width and
 *    height, and fails on every prefix that ends before them;
 *  - a whole PNG and JPEG load at their size (the PNG's pixels exact, the
 *    JPEG's near the saved ones); a JPEG decodes at 1/2, 1/4 and 1/8 of its
 *    size, each side rounded up; an image of more pixels than maxPixels (as it
 *    would be decoded) fails, and the same JPEG at a scale that fits loads;
 *  - every proper prefix of the JPEG fails (libjpeg's premature-end warning is
 *    a failure, not grey rows), and so does every prefix of the PNG that ends
 *    before its last IDAT;
 *  - the fuzz: each byte of both files changed in turn (XOR 0xFF and 0x01)
 *    loads or fails but never aborts, and a load gives a non-empty image
 *    within the pixel limit;
 *  - a PNG whose IHDR says 65537x65537 (a valid one: its CRC right, rows of
 *    data after it) fails under a 64 MP maxPixels with nothing allocated: the
 *    product is 2^32+131073 pixels, which a 32-bit size_t (wasm32) wraps to
 *    131073, so this is a real check in the web tree.
 *  Every buffer is copied into an allocation of exactly its size, so in the
 *  ASAN tree a read past its end is a reported over-read.
 */
#include <gweni/utils/filesystem.h>

#include <imglib/imageIo.h>
#include <imglib/jpeg.h>
#include <imglib/png.h>
#include <imglib/probe.h>
#include <imglib/simpleImage.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

namespace
{

constexpr size_t PngWidth=40;
constexpr size_t PngHeight=30;
//odd sizes, so the reduced decodes round up
constexpr size_t JpegWidth=57;
constexpr size_t JpegHeight=41;
//the bound the fuzz loads under: a changed header byte can ask for 65535 rows
constexpr size_t FuzzMaxPixels=size_t(1) << 24;

//! A smooth image (a JPEG of noise is all artefacts) of the format.
imglib::SimpleImage gradient(imglib::Format format, size_t width, size_t height)
{
    const size_t channels=(format == imglib::Format::RGBA)?4:((format == imglib::Format::RGB)?3:1);
    imglib::SimpleImage image(format, imglib::Depth::Bit8, width, height);

    for(size_t y=0; y<height; ++y)
    {
        for(size_t x=0; x<width; ++x)
        {
            uint8_t *texel=image.data+(y*width+x)*channels;

            for(size_t c=0; c<channels; ++c)
                texel[c]=(uint8_t)((c == 3)?(128+x):((x*200/width)+(y*50/height)+c*10));
        }
    }
    return image;
}

std::vector<uint8_t> readFile(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);

    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

//! The bytes imglib saves an image as, through a file under the temp dir.
std::vector<uint8_t> encode(imglib::SimpleImage &image, const char *extension)
{
    const fs::path path=fs::temp_directory_path()/
        ("gweni_imageBuffer_"+std::to_string(getpid())+extension);
    const bool saved=(std::string(extension) == ".png")?imglib::savePng(image, path.string().c_str()):
        imglib::saveJpeg(image, path.string().c_str());

    REQUIRE(saved);

    std::vector<uint8_t> bytes=readFile(path);
    std::error_code error;

    fs::remove(path, error);
    REQUIRE(!bytes.empty());
    return bytes;
}

//! An RGBA image of noise, so its PNG's IDAT is a few KB for the fuzz to change.
imglib::SimpleImage noise(size_t width, size_t height)
{
    imglib::SimpleImage image(imglib::Format::RGBA, imglib::Depth::Bit8, width, height);
    uint32_t state=0x12345678u;

    for(size_t i=0; i<image.dataSize; ++i)
    {
        state^=state << 13;
        state^=state >> 17;
        state^=state << 5;
        image.data[i]=(uint8_t)(state >> 24);
    }
    return image;
}

struct Encoded
{
    imglib::SimpleImage pngImage=noise(PngWidth, PngHeight);
    imglib::SimpleImage jpegImage=gradient(imglib::Format::RGB, JpegWidth, JpegHeight);
    std::vector<uint8_t> png=encode(pngImage, ".png");
    std::vector<uint8_t> jpeg=encode(jpegImage, ".jpg");
};

//! The first size bytes of bytes in an allocation of exactly that size.
std::unique_ptr<uint8_t[]> copyOf(const std::vector<uint8_t> &bytes, size_t size)
{
    std::unique_ptr<uint8_t[]> copy(new uint8_t[size]);

    if(size > 0)
        memcpy(copy.get(), bytes.data(), size);
    return copy;
}

bool loadPrefix(const std::vector<uint8_t> &bytes, size_t size, imglib::SimpleImage &image,
    const imglib::BufferLoadOptions &options=imglib::BufferLoadOptions())
{
    std::unique_ptr<uint8_t[]> copy=copyOf(bytes, size);

    return imglib::loadBuffer(image, copy.get(), size, options);
}

bool probePrefix(const std::vector<uint8_t> &bytes, size_t size, imglib::ImageHeader &header)
{
    std::unique_ptr<uint8_t[]> copy=copyOf(bytes, size);

    return imglib::probeImage(copy.get(), size, header);
}

//! The offset past the last IDAT chunk's CRC (the chunks walked by their headers)
size_t lastIdatEnd(const std::vector<uint8_t> &png)
{
    size_t pos=8;
    size_t end=0;

    while(pos+12 <= png.size())
    {
        const uint8_t *p=&png[pos];
        const size_t length=(size_t(p[0]) << 24)|(size_t(p[1]) << 16)|(size_t(p[2]) << 8)|size_t(p[3]);

        if(memcmp(p+4, "IDAT", 4) == 0)
            end=pos+12+length;
        pos+=12+length;
    }
    return end;
}

//! The CRC-32 of a PNG chunk's type and data (the PNG/zlib polynomial)
uint32_t crc32(const uint8_t *data, size_t size, uint32_t crc=0)
{
    crc=~crc;
    for(size_t i=0; i<size; ++i)
    {
        crc^=data[i];
        for(int bit=0; bit<8; ++bit)
            crc=(crc >> 1)^(0xedb88320u & (0u-(crc & 1u)));
    }
    return ~crc;
}

void putBig32(std::vector<uint8_t> &out, uint32_t value)
{
    for(int shift=24; shift>=0; shift-=8)
        out.push_back((uint8_t)(value >> shift));
}

void putChunk(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> body(type, type+4);

    body.insert(body.end(), data.begin(), data.end());
    putBig32(out, (uint32_t)data.size());
    out.insert(out.end(), body.begin(), body.end());
    putBig32(out, crc32(body.data(), body.size()));
}

//! A well-formed RGBA PNG whose IHDR says width x height, holding the first
//! rows rows of it (filter 0, every byte 0x7f) in stored deflate blocks.
std::vector<uint8_t> craftedPng(uint32_t width, uint32_t height, size_t rows)
{
    std::vector<uint8_t> raw;

    for(size_t row=0; row<rows; ++row)
    {
        raw.push_back(0);
        raw.insert(raw.end(), (size_t)width*4, 0x7f);
    }

    std::vector<uint8_t> zlib={ 0x78, 0x01 };
    uint32_t a=1;
    uint32_t b=0;

    for(size_t pos=0; pos < raw.size(); pos+=65535)
    {
        const size_t length=std::min<size_t>(65535, raw.size()-pos);

        zlib.push_back((pos+length == raw.size())?1:0);
        zlib.push_back((uint8_t)length);
        zlib.push_back((uint8_t)(length >> 8));
        zlib.push_back((uint8_t)~length);
        zlib.push_back((uint8_t)(~length >> 8));
        zlib.insert(zlib.end(), raw.begin()+pos, raw.begin()+pos+length);
    }
    for(uint8_t byte:raw)
    {
        a=(a+byte)%65521;
        b=(b+a)%65521;
    }
    putBig32(zlib, (b << 16)|a);

    std::vector<uint8_t> ihdr;

    putBig32(ihdr, width);
    putBig32(ihdr, height);
    ihdr.insert(ihdr.end(), { 8, 6, 0, 0, 0 });

    std::vector<uint8_t> png={ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

    putChunk(png, "IHDR", ihdr);
    putChunk(png, "IDAT", zlib);
    putChunk(png, "IEND", {});
    return png;
}

double meanDifference(const imglib::SimpleImage &a, const imglib::SimpleImage &b)
{
    double sum=0.0;

    for(size_t i=0; i<a.dataSize; ++i)
        sum+=std::abs((int)a.data[i]-(int)b.data[i]);
    return sum/(double)a.dataSize;
}

}//namespace

TEST_CASE("imageBuffer: the format is found from the first bytes, and unknown bytes do not load")
{
    Encoded files;
    const uint8_t gif[]={ 'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0, 0, 0, 0 };
    const char *text="<html>not an image</html>";
    const uint8_t soiOnly[]={ 0xff, 0xd8 };

    CHECK(imglib::detectFormat(files.png.data(), files.png.size()) == imglib::FileFormat::Png);
    CHECK(imglib::detectFormat(files.jpeg.data(), files.jpeg.size()) == imglib::FileFormat::Jpeg);
    CHECK(imglib::detectFormat(gif, sizeof(gif)) == imglib::FileFormat::Unknown);
    CHECK(imglib::detectFormat((const uint8_t *)text, strlen(text)) == imglib::FileFormat::Unknown);
    CHECK(imglib::detectFormat(soiOnly, sizeof(soiOnly)) == imglib::FileFormat::Unknown);
    CHECK(imglib::detectFormat(files.png.data(), 7) == imglib::FileFormat::Unknown);
    CHECK(imglib::detectFormat(nullptr, 100) == imglib::FileFormat::Unknown);

    imglib::SimpleImage image;
    imglib::ImageHeader header;

    CHECK_FALSE(imglib::loadBuffer(image, gif, sizeof(gif)));
    CHECK_FALSE(imglib::loadBuffer(image, (const uint8_t *)text, strlen(text)));
    CHECK_FALSE(imglib::loadBuffer(image, soiOnly, sizeof(soiOnly)));
    CHECK_FALSE(imglib::loadBuffer(image, nullptr, 0));
    CHECK_FALSE(imglib::probeImage(gif, sizeof(gif), header));
    CHECK(header.format == imglib::FileFormat::Unknown);

    // noise of a few sizes, its first byte kept off both signatures
    uint32_t state=0x9e3779b9u;

    for(size_t size:{ 1u, 3u, 8u, 64u, 4096u })
    {
        std::vector<uint8_t> noise(size);

        for(uint8_t &byte:noise)
        {
            state^=state << 13;
            state^=state >> 17;
            state^=state << 5;
            byte=(uint8_t)(state >> 24);
        }
        noise[0]=0x42;
        CAPTURE(size);
        CHECK_FALSE(loadPrefix(noise, size, image));
    }
}

TEST_CASE("imageBuffer: probeImage reads the size from the header and fails on every prefix before it")
{
    Encoded files;
    imglib::ImageHeader header;

    REQUIRE(imglib::probeImage(files.png.data(), files.png.size(), header));
    CHECK(header.format == imglib::FileFormat::Png);
    CHECK(header.width == PngWidth);
    CHECK(header.height == PngHeight);

    REQUIRE(imglib::probeImage(files.jpeg.data(), files.jpeg.size(), header));
    CHECK(header.format == imglib::FileFormat::Jpeg);
    CHECK(header.width == JpegWidth);
    CHECK(header.height == JpegHeight);

    for(const std::vector<uint8_t> *bytes:{ &files.png, &files.jpeg })
    {
        // the shortest prefix the probe reads the size from; every longer one does too
        size_t first=0;

        while(first < bytes->size() && !probePrefix(*bytes, first, header))
            ++first;
        CAPTURE(first);
        REQUIRE(first < bytes->size());
        CHECK(first >= ((bytes == &files.png)?24u:11u));
        for(size_t size=first; size <= bytes->size(); size+=7)
        {
            CAPTURE(size);
            CHECK(probePrefix(*bytes, size, header));
        }
    }
}

TEST_CASE("imageBuffer: a whole PNG and JPEG load at their size, the JPEG at 1/2, 1/4 and 1/8 too")
{
    Encoded files;
    imglib::SimpleImage png;
    imglib::SimpleImage jpeg;

    REQUIRE(loadPrefix(files.png, files.png.size(), png));
    CHECK(png.format == imglib::Format::RGBA);
    CHECK(png.width == PngWidth);
    CHECK(png.height == PngHeight);
    REQUIRE(png.dataSize == files.pngImage.dataSize);
    CHECK(memcmp(png.data, files.pngImage.data, png.dataSize) == 0);

    REQUIRE(loadPrefix(files.jpeg, files.jpeg.size(), jpeg));
    CHECK(jpeg.format == imglib::Format::RGB);
    CHECK(jpeg.width == JpegWidth);
    CHECK(jpeg.height == JpegHeight);
    REQUIRE(jpeg.dataSize == files.jpegImage.dataSize);
    CHECK(meanDifference(jpeg, files.jpegImage) < 4.0);

    for(unsigned int denom:{ 2u, 4u, 8u })
    {
        CAPTURE(denom);
        imglib::SimpleImage reduced;
        imglib::BufferLoadOptions options;

        options.scaleDenom=denom;
        REQUIRE(loadPrefix(files.jpeg, files.jpeg.size(), reduced, options));
        CHECK(reduced.width == (JpegWidth+denom-1)/denom);
        CHECK(reduced.height == (JpegHeight+denom-1)/denom);
    }

    // a grey JPEG loads as grey
    imglib::SimpleImage grey=gradient(imglib::Format::GreyScale, 20, 10);
    const std::vector<uint8_t> greyJpeg=encode(grey, ".jpg");
    imglib::SimpleImage greyLoaded;

    REQUIRE(loadPrefix(greyJpeg, greyJpeg.size(), greyLoaded));
    CHECK(greyLoaded.format == imglib::Format::GreyScale);
    CHECK(greyLoaded.width == 20);
}

TEST_CASE("imageBuffer: an image of more pixels than maxPixels fails, and a JPEG reduced under it loads")
{
    Encoded files;
    imglib::SimpleImage image;
    imglib::BufferLoadOptions options;

    options.maxPixels=PngWidth*PngHeight-1;
    CHECK_FALSE(loadPrefix(files.png, files.png.size(), image, options));
    options.maxPixels=PngWidth*PngHeight;
    CHECK(loadPrefix(files.png, files.png.size(), image, options));

    options.maxPixels=JpegWidth*JpegHeight-1;
    CHECK_FALSE(loadPrefix(files.jpeg, files.jpeg.size(), image, options));
    // 1/2: 29 x 21
    options.scaleDenom=2;
    options.maxPixels=29*21;
    REQUIRE(loadPrefix(files.jpeg, files.jpeg.size(), image, options));
    CHECK(image.width == 29);
    options.maxPixels=29*21-1;
    CHECK_FALSE(loadPrefix(files.jpeg, files.jpeg.size(), image, options));
}

TEST_CASE("imageBuffer: a 65537x65537 PNG header fails a 64 MP maxPixels with nothing allocated, in 32 bits too")
{
    // the crafted file is a real PNG: at a size within the limit it loads
    {
        const std::vector<uint8_t> png=craftedPng(16, 4, 4);
        imglib::SimpleImage image;

        REQUIRE(loadPrefix(png, png.size(), image));
        CHECK(image.width == 16);
        CHECK(image.height == 4);
    }

    const std::vector<uint8_t> png=craftedPng(65537, 65537, 4);
    imglib::BufferLoadOptions options;
    imglib::SimpleImage image;

    options.maxPixels=size_t(64) << 20;
    CHECK_FALSE(loadPrefix(png, png.size(), image, options));
    CHECK(image.dataSize == 0);
    CHECK(image.data == nullptr);

    imglib::ImageHeader header;

    REQUIRE(probePrefix(png, png.size(), header));
    CHECK(header.width == 65537);
    CHECK(header.height == 65537);
}

TEST_CASE("imageBuffer: every truncated JPEG, and every PNG cut before its last IDAT ends, fails")
{
    Encoded files;
    imglib::SimpleImage image;
    size_t loaded=0;

    for(size_t size=0; size < files.jpeg.size(); ++size)
    {
        if(loadPrefix(files.jpeg, size, image))
        {
            MESSAGE("a JPEG prefix of ", size, " of ", files.jpeg.size(), " bytes loaded");
            ++loaded;
        }
    }

    const size_t idatEnd=lastIdatEnd(files.png);

    REQUIRE(idatEnd > 8);
    for(size_t size=0; size < idatEnd; ++size)
    {
        if(loadPrefix(files.png, size, image))
        {
            MESSAGE("a PNG prefix of ", size, " of ", idatEnd, " bytes loaded");
            ++loaded;
        }
    }
    CHECK(loaded == 0);
}

TEST_CASE("imageBuffer: each byte changed in turn loads or fails, never aborts, and a load is a real image")
{
    Encoded files;
    imglib::BufferLoadOptions options;
    size_t loads=0;
    size_t failures=0;

    options.maxPixels=FuzzMaxPixels;
    for(const std::vector<uint8_t> *bytes:{ &files.png, &files.jpeg })
    {
        for(uint8_t mask:{ (uint8_t)0xff, (uint8_t)0x01 })
        {
            for(size_t pos=0; pos < bytes->size(); ++pos)
            {
                std::unique_ptr<uint8_t[]> corrupt=copyOf(*bytes, bytes->size());
                imglib::SimpleImage image;

                corrupt[pos]^=mask;
                if(imglib::loadBuffer(image, corrupt.get(), bytes->size(), options))
                {
                    ++loads;
                    if(!(image.width > 0 && image.height > 0 && image.width*image.height <= FuzzMaxPixels &&
                        image.data != nullptr))
                    {
                        CAPTURE(pos);
                        CAPTURE(int(mask));
                        CHECK_MESSAGE(false, "a load gave a ", image.width, "x", image.height, " image");
                    }
                }
                else
                    ++failures;
            }
        }
    }
    MESSAGE("png ", files.png.size(), " bytes, jpeg ", files.jpeg.size(), " bytes: ", loads, " loads, ", failures, " failures");
    // both happen: a changed CRC or marker fails, a changed pixel byte of the JPEG loads
    CHECK(loads > 0);
    CHECK(failures > 0);
}
