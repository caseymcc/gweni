/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's image loads from memory (IResourceLoader::loadTexture's bytes
 *  overload and loadTexturePixels), on every backend:
 *  - a PNG's bytes load with its own texels, a JPEG's with near ones, and rows
 *    of RGBA8 texels a stride apart with exactly theirs; each draws them;
 *  - bad bytes (none, unknown, a corrupt header, a truncated PNG or JPEG) and
 *    bad pixels (no buffer, a short stride) are ErrorBadData with exactly one
 *    log line each, leave the texture unloaded and draw the missing image;
 *  - maxSize keeps the largest level of the mip chain that fits it: 4000x3000
 *    at 512 keeps 500x375, a JPEG decoded at 1/4 (1000x750, lastDecodedSize)
 *    so its full size is never allocated, a PNG decoded whole;
 *  - the decode limits, from the header before anything is decoded: a header
 *    over 64 megapixels fails (lastDecodedSize untouched), 65537x65537 too
 *    (a product a 32-bit size_t wraps under the limit), and so does an
 *    image larger than the largest image page with no maxSize, which loads
 *    with one;
 *  - loadedTextureCount() counts the loads not freed, back at its baseline
 *    once they are freed, and a failed load counts nothing.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/platforms/commonPlatform.h>
#include <gweni/utils/filesystem.h>

#include <imglib/jpeg.h>
#include <imglib/png.h>
#include <imglib/simpleImage.h>

#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;
using gweni::Texture;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);

//! The log lines written while it lives, counted
struct LogCount
{
    LogCount() { lines()=0; previous=gweni::log::setLogCaller(&count); }
    ~LogCount() { gweni::log::setLogCaller(previous); }

    static void count(gweni::log::Level, const char *message) { ++lines(); last()=message; }
    static size_t &lines() { static size_t value=0; return value; }
    //! the last line written
    static std::string &last() { static std::string value; return value; }

    gweni::log::LogCaller previous;
};

//! Texel (x, y) of an image of seed: smooth, so a JPEG keeps it nearly
Color texelOf(int seed, int x, int y, int width, int height)
{
    return Color((uint8_t)(seed+x*100/width), (uint8_t)(y*200/height), (uint8_t)(255-x*80/width), 255);
}

imglib::SimpleImage makeImage(int seed, int width, int height, imglib::Format format=imglib::Format::RGBA)
{
    const size_t channels=(format == imglib::Format::RGBA)?4:3;
    imglib::SimpleImage image(format, imglib::Depth::Bit8, width, height);

    for(int y=0; y<height; ++y)
    {
        for(int x=0; x<width; ++x)
        {
            const Color c=texelOf(seed, x, y, width, height);
            uint8_t *t=image.data+((size_t)y*width+x)*channels;

            t[0]=c.r;
            t[1]=c.g;
            t[2]=c.b;
            if(channels == 4)
                t[3]=c.a;
        }
    }
    return image;
}

//! The bytes imglib saves an image as (".png" or ".jpg"), through a temp file
std::vector<uint8_t> encode(imglib::SimpleImage &image, const char *extension)
{
    const fs::path path=fs::temp_directory_path()/("gweni_imageBytes_"+std::to_string(getpid())+extension);
    const bool saved=(std::string(extension) == ".png")?imglib::savePng(image, path.string().c_str()):
        imglib::saveJpeg(image, path.string().c_str());

    REQUIRE(saved);

    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::error_code error;

    fs::remove(path, error);
    REQUIRE(!bytes.empty());
    return bytes;
}

//! A loaded texture, freed when the case ends
struct Loaded
{
    Loaded() { texture.readable=true; }
    ~Loaded() { gweni::test::gpuRenderer().freeTexture(texture); }

    Texture texture;
    Texture::Status status=Texture::Status::Unloaded;

    bool loaded() const { return status == Texture::Status::Loaded; }
    gweni::TextureData data() const { return gweni::test::gpuRenderer().getTextureData(texture); }
};

//! A primitive id for one case, released (and that flushed) at its end
struct Primitive
{
    Primitive() { gweni::test::gpuRenderer().generatePrimitive(id); }
    ~Primitive()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
    }

    size_t id=0;
};

//! Clears, draws the texture 1:1 at (8, 8) and reads the frame back (top-down RGBA8)
std::vector<uint8_t> drawImage(const Texture &texture, int width, int height)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    Primitive prim;
    std::vector<uint8_t> pixels;

    gweni::test::clearView(background);
    renderer.begin();
    renderer.setDrawColor(Color(255, 255, 255, 255));
    renderer.drawTexturedRect(texture, prim.id, Rect(8, 8, width, height), 1);
    renderer.end();
    gweni::test::readView(pixels);
    return pixels;
}

//! Whether the last load decoded its pixels at width x height
bool decodedAt(int width, int height)
{
    const gweni::Point size=gweni::test::gpuRenderer().lastDecodedSize();

    if(size.x != width || size.y != height)
        MESSAGE("decoded at ", size.x, "x", size.y, ", expected ", width, "x", height);
    return size.x == width && size.y == height;
}

Color at(const std::vector<uint8_t> &pixels, int x, int y)
{
    const uint8_t *p=&pixels[((size_t)y*gweni::test::viewSize().x+x)*4];

    return Color(p[0], p[1], p[2], p[3]);
}

//! Pixels of the w x h block at (8, 8) more than tolerance from expected(x, y)
template<typename Expected>
int mismatches(const std::vector<uint8_t> &pixels, int w, int h, int tolerance, Expected &&expected)
{
    int count=0;

    for(int y=0; y<h; ++y)
    {
        for(int x=0; x<w; ++x)
        {
            const Color p=at(pixels, 8+x, 8+y);
            const Color e=expected(x, y);

            if(std::abs(p.r-e.r) > tolerance || std::abs(p.g-e.g) > tolerance || std::abs(p.b-e.b) > tolerance)
                ++count;
        }
    }
    return count;
}

}//namespace

TEST_CASE("Image bytes: a PNG's, a JPEG's and RGBA pixels' texels load and draw")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const size_t baseline=renderer.loadedTextureCount();
    imglib::SimpleImage pngImage=makeImage(10, 48, 32);
    imglib::SimpleImage jpegImage=makeImage(60, 40, 24, imglib::Format::RGB);
    const std::vector<uint8_t> png=encode(pngImage, ".png");
    const std::vector<uint8_t> jpeg=encode(jpegImage, ".jpg");

    {
        Loaded fromPng;
        Loaded fromJpeg;
        Loaded fromPixels;

        fromPng.status=renderer.loadTexture(fromPng.texture, png.data(), png.size());
        fromJpeg.status=renderer.loadTexture(fromJpeg.texture, jpeg.data(), jpeg.size());

        // 30x20 texels in rows of 33 texels: the stride's padding is not copied
        constexpr int w=30;
        constexpr int h=20;
        constexpr size_t stride=33*4;
        std::vector<uint8_t> rows(stride*h, 0xee);

        for(int y=0; y<h; ++y)
        {
            for(int x=0; x<w; ++x)
            {
                const Color c=texelOf(120, x, y, w, h);
                uint8_t *t=&rows[y*stride+x*4];

                t[0]=c.r;
                t[1]=c.g;
                t[2]=c.b;
                t[3]=c.a;
            }
        }
        fromPixels.status=renderer.loadTexturePixels(fromPixels.texture, w, h, stride, rows.data());

        REQUIRE(fromPng.loaded());
        REQUIRE(fromJpeg.loaded());
        REQUIRE(fromPixels.loaded());
        CHECK(renderer.loadedTextureCount() == baseline+3);
        CHECK(fromPng.data().width == 48);
        CHECK(fromPng.data().height == 32);
        CHECK(fromJpeg.data().width == 40);
        CHECK(fromJpeg.data().height == 24);
        CHECK(fromPixels.data().width == w);

        for(int y=0; y<h; ++y)
        {
            for(int x=0; x<w; ++x)
            {
                const Color texel=renderer.pixelColor(fromPixels.texture, x, y, Color());

                if(!(texel == texelOf(120, x, y, w, h)))
                {
                    CAPTURE(x);
                    CAPTURE(y);
                    CHECK_MESSAGE(false, "a pixel texel differs");
                }
            }
        }

        const std::vector<uint8_t> pngFrame=drawImage(fromPng.texture, 48, 32);
        CHECK(mismatches(pngFrame, 48, 32, 0, [](int x, int y) { return texelOf(10, x, y, 48, 32); }) == 0);

        const std::vector<uint8_t> jpegFrame=drawImage(fromJpeg.texture, 40, 24);
        CHECK(mismatches(jpegFrame, 40, 24, 12, [](int x, int y) { return texelOf(60, x, y, 40, 24); }) == 0);

        const std::vector<uint8_t> pixelFrame=drawImage(fromPixels.texture, w, h);
        CHECK(mismatches(pixelFrame, w, h, 0, [](int x, int y) { return texelOf(120, x, y, w, h); }) == 0);
    }
    CHECK(renderer.loadedTextureCount() == baseline);
}

TEST_CASE("Image bytes: bad bytes and pixels are ErrorBadData with one log line, unloaded, drawn as missing")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const size_t baseline=renderer.loadedTextureCount();
    imglib::SimpleImage pngImage=makeImage(10, 48, 32);
    imglib::SimpleImage jpegImage=makeImage(60, 40, 24, imglib::Format::RGB);
    const std::vector<uint8_t> png=encode(pngImage, ".png");
    const std::vector<uint8_t> jpeg=encode(jpegImage, ".jpg");
    const char *text="<html>404</html>";
    std::vector<uint8_t> badHeader(png);

    // IHDR's width 0
    memset(&badHeader[16], 0, 4);

    struct Bad
    {
        const char *what;
        const uint8_t *bytes;
        size_t size;
    };
    const Bad bads[]=
    {
        { "no bytes", nullptr, 0 },
        { "unknown bytes", (const uint8_t *)text, strlen(text) },
        { "a corrupt PNG header", badHeader.data(), badHeader.size() },
        { "a truncated PNG", png.data(), png.size()/2 },
        { "a truncated JPEG", jpeg.data(), jpeg.size()-40 },
        { "a JPEG of its header alone", jpeg.data(), 30 },
    };

    for(const Bad &bad:bads)
    {
        INFO(bad.what);
        Loaded image;
        LogCount log;

        image.status=renderer.loadTexture(image.texture, bad.bytes, bad.size);
        CHECK((image.status == Texture::Status::ErrorBadData));
        CHECK(LogCount::lines() == 1);
        CHECK(image.texture.dataIndex == std::numeric_limits<size_t>::max());
        CHECK_FALSE(renderer.ensureTexture(image.texture));
        CHECK(renderer.loadedTextureCount() == baseline);

        const std::vector<uint8_t> frame=drawImage(image.texture, 16, 16);

        CHECK(mismatches(frame, 16, 16, 0, [](int, int) { return gweni::colors::Missing; }) == 0);
    }

    const uint8_t rgba[16]={};
    Loaded image;
    LogCount log;

    image.status=renderer.loadTexturePixels(image.texture, 4, 1, 15, rgba);
    CHECK((image.status == Texture::Status::ErrorBadData));
    image.status=renderer.loadTexturePixels(image.texture, 4, 1, 16, nullptr);
    CHECK((image.status == Texture::Status::ErrorBadData));
    image.status=renderer.loadTexturePixels(image.texture, 0, 1, 16, rgba);
    CHECK((image.status == Texture::Status::ErrorBadData));
    CHECK(LogCount::lines() == 3);
    CHECK(renderer.loadedTextureCount() == baseline);
}

TEST_CASE("Image bytes: maxSize keeps the largest mip level that fits, a JPEG decoded reduced")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    imglib::SimpleImage big=makeImage(0, 4000, 3000, imglib::Format::RGB);

    {
        const std::vector<uint8_t> jpeg=encode(big, ".jpg");
        Loaded image;

        image.status=renderer.loadTexture(image.texture, jpeg.data(), jpeg.size(), 512);
        REQUIRE(image.loaded());
        // decoded at 1/4, the smallest that still holds 512, then one level down
        CHECK(decodedAt(1000, 750));
        CHECK(image.data().width == 500);
        CHECK(image.data().height == 375);
    }
    {
        const std::vector<uint8_t> png=encode(big, ".png");
        Loaded image;

        image.status=renderer.loadTexture(image.texture, png.data(), png.size(), 512);
        REQUIRE(image.loaded());
        CHECK(decodedAt(4000, 3000));
        CHECK(image.data().width == 500);
        CHECK(image.data().height == 375);
    }
    {
        Loaded image;

        image.status=renderer.loadTexturePixels(image.texture, 4000, 3000, 4000*3, big.data, 512);
        CHECK((image.status == Texture::Status::ErrorBadData));
    }

    // RGBA rows of the same size, kept at 1000 (a side of 1000 fits at level 2)
    std::vector<uint8_t> rgba((size_t)4000*3000*4, 128);
    Loaded image;

    image.status=renderer.loadTexturePixels(image.texture, 4000, 3000, 4000*4, rgba.data(), 1000);
    REQUIRE(image.loaded());
    CHECK(image.data().width == 1000);
    CHECK(image.data().height == 750);
}

TEST_CASE("Image bytes: over 64 megapixels, or past the largest page without a max size, fails from the header")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const size_t baseline=renderer.loadedTextureCount();

    // a PNG header of 9000x8000 (72 MP) and nothing after it: the probe reads
    // the size, so nothing is decoded and lastDecodedSize stays
    imglib::SimpleImage small=makeImage(0, 8, 8);
    std::vector<uint8_t> png=encode(small, ".png");
    const uint8_t huge[8]={ 0, 0, 0x23, 0x28, 0, 0, 0x1f, 0x40 };

    memcpy(&png[16], huge, 8);
    png.resize(33);

    {
        Loaded marker;
        const std::vector<uint8_t> markerPng=encode(small, ".png");

        marker.status=renderer.loadTexture(marker.texture, markerPng.data(), markerPng.size());
        REQUIRE(marker.loaded());
    }
    REQUIRE(decodedAt(8, 8));

    {
        Loaded image;
        LogCount log;

        image.status=renderer.loadTexture(image.texture, png.data(), png.size(), 512);
        CHECK((image.status == Texture::Status::ErrorBadData));
        CHECK(LogCount::lines() == 1);
        CHECK(LogCount::last().find("64 megapixel") != std::string::npos);
        CHECK(decodedAt(8, 8));
    }
    // 65537x65537 is 2^32+131073 pixels, which a 32-bit size_t (wasm32)
    // wraps to 131073: the limit is checked in 64 bits
    {
        const uint8_t wraps[8]={ 0, 1, 0, 1, 0, 1, 0, 1 };
        Loaded image;
        LogCount log;

        memcpy(&png[16], wraps, 8);
        image.status=renderer.loadTexture(image.texture, png.data(), png.size(), 512);
        CHECK((image.status == Texture::Status::ErrorBadData));
        CHECK(LogCount::lines() == 1);
        CHECK(LogCount::last().find("64 megapixel") != std::string::npos);
        CHECK(decodedAt(8, 8));
    }

    // 3000x10 is over the largest page (2048): it loads only with a max size
    imglib::SimpleImage wide=makeImage(0, 3000, 10);
    const std::vector<uint8_t> widePng=encode(wide, ".png");

    {
        Loaded image;
        LogCount log;

        image.status=renderer.loadTexture(image.texture, widePng.data(), widePng.size());
        CHECK((image.status == Texture::Status::ErrorBadData));
        CHECK(LogCount::lines() == 1);
        CHECK(LogCount::last().find("no max size") != std::string::npos);
        CHECK(decodedAt(8, 8));
    }
    {
        Loaded image;

        image.status=renderer.loadTexture(image.texture, widePng.data(), widePng.size(), 2048);
        REQUIRE(image.loaded());
        CHECK(image.data().width == 1500);
        CHECK(image.data().height == 5);
    }
    CHECK(renderer.loadedTextureCount() == baseline);
}
