/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  imglib::loadPngBuffer on a buffer holding less than the PNG in it says:
 *  every prefix of a valid PNG that stops before the end of its last IDAT
 *  chunk returns false without aborting (0 bytes, 1-7, exactly the 8-byte
 *  signature, cuts inside IHDR, IDAT's first byte, middle and one byte short
 *  of its CRC's end), and so does a null buffer. The whole buffer still loads
 *  as loadPng loads the same file: the format, depth, size and pixels of the
 *  image that was saved.
 *
 *  readBuffer, the libpng read callback, used to assert that the bytes were
 *  there and memcpy them anyway, and png_sig_cmp read 8 bytes of a shorter
 *  buffer: an abort in debug, a heap over-read in release. Each prefix here is
 *  copied into its own heap allocation of exactly its size, so an over-read is
 *  past the allocation, and this is a real over-read check in the ASAN tree.
 *
 *  A prefix that keeps every IDAT byte and loses only (part of) IEND may load:
 *  the loader reads the rows and never reads IEND.
 */
#include <gweni/utils/filesystem.h>

#include <imglib/png.h>
#include <imglib/simpleImage.h>

#include <doctest/doctest.h>

#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

namespace
{

struct Chunk
{
    std::string type;
    size_t offset; //of its length field
    size_t length; //of its data

    size_t dataOffset() const { return offset+8; }
    size_t end() const { return offset+12+length; } //past its CRC
};

//! The chunks after the signature, walked by their headers (4-byte big-endian
//! length, 4-byte type, the data, a 4-byte CRC).
std::vector<Chunk> chunks(const std::vector<uint8_t> &png)
{
    std::vector<Chunk> list;
    size_t pos=8;

    while(pos+12<=png.size())
    {
        const uint8_t *p=&png[pos];
        size_t length=(size_t(p[0])<<24)|(size_t(p[1])<<16)|(size_t(p[2])<<8)|size_t(p[3]);

        list.push_back({std::string((const char *)p+4, 4), pos, length});
        pos+=12+length;
    }
    return list;
}

//! A 40x40 RGBA image of noise, so its IDAT is a few KB.
imglib::SimpleImage testImage()
{
    imglib::SimpleImage image(imglib::Format::RGBA, imglib::Depth::Bit8, 40, 40);
    uint32_t state=0x12345678u;

    for(size_t i=0; i<image.dataSize; ++i)
    {
        state^=state<<13;
        state^=state>>17;
        state^=state<<5;
        image.data[i]=(uint8_t)(state>>24);
    }
    return image;
}

//! A file under the temp dir, removed again on destruction.
struct TempFile
{
    TempFile():
        path(fs::temp_directory_path()/("gweni_pngBuffer_"+std::to_string(getpid())+".png"))
    {}

    ~TempFile()
    {
        std::error_code ec;
        fs::remove(path, ec);
    }

    fs::path path;
};

std::vector<uint8_t> readFile(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

//! loadPngBuffer on the first size bytes of png, in an allocation of exactly
//! that size (new[] of 0 included, which is not null).
bool loadPrefix(const std::vector<uint8_t> &png, size_t size)
{
    std::unique_ptr<uint8_t[]> prefix(new uint8_t[size]);

    if(size>0)
        memcpy(prefix.get(), png.data(), size);

    imglib::SimpleImage image;
    return imglib::loadPngBuffer(image, prefix.get(), size);
}

bool samePixels(const imglib::SimpleImage &a, const imglib::SimpleImage &b)
{
    return (a.format==b.format)&&(a.depth==b.depth)&&(a.width==b.width)&&(a.height==b.height)&&
        (a.dataSize==b.dataSize)&&(memcmp(a.data, b.data, a.dataSize)==0);
}

struct SavedPng
{
    imglib::SimpleImage image=testImage();
    TempFile file;
    std::vector<uint8_t> bytes;
    std::vector<Chunk> list;

    SavedPng()
    {
        REQUIRE(imglib::savePng(image, file.path.string().c_str()));
        bytes=readFile(file.path);
        list=chunks(bytes);
    }

    const Chunk &first(const char *type) const
    {
        for(const Chunk &chunk:list)
        {
            if(chunk.type==type)
                return chunk;
        }
        FAIL("no ", type, " chunk");
        return list.front();
    }

    //! Past the CRC of the last IDAT chunk.
    size_t idatEnd() const
    {
        size_t end=0;

        for(const Chunk &chunk:list)
        {
            if(chunk.type=="IDAT")
                end=chunk.end();
        }
        return end;
    }
};

}//namespace

TEST_CASE("pngBuffer: the test PNG has the chunks the cuts are placed in")
{
    SavedPng png;

    REQUIRE(png.list.size()>=3);
    CHECK(png.list.front().type=="IHDR");
    CHECK(png.list.front().length==13);
    CHECK(png.list.back().type=="IEND");
    CHECK(png.list.back().end()==png.bytes.size());
    //big enough that a cut in its middle is well inside the compressed rows
    CHECK(png.first("IDAT").length>1000);
}

TEST_CASE("pngBuffer: a null buffer and a buffer shorter than the signature return false")
{
    SavedPng png;
    imglib::SimpleImage image;

    CHECK_FALSE(imglib::loadPngBuffer(image, nullptr, 0));
    CHECK_FALSE(imglib::loadPngBuffer(image, nullptr, png.bytes.size()));

    for(size_t size=0; size<8; ++size)
    {
        CAPTURE(size);
        CHECK_FALSE(loadPrefix(png.bytes, size));
    }
}

TEST_CASE("pngBuffer: a buffer cut in the signature's end, IHDR or IDAT returns false")
{
    SavedPng png;
    const Chunk &ihdr=png.first("IHDR");
    const Chunk &idat=png.first("IDAT");

    struct Cut
    {
        const char *where;
        size_t size;
    };
    const Cut cuts[]=
    {
        {"exactly the signature", 8},
        {"inside IHDR's length", ihdr.offset+2},
        {"inside IHDR's type", ihdr.offset+6},
        {"inside IHDR's data", ihdr.dataOffset()+5},
        {"inside IHDR's CRC", ihdr.end()-1},
        {"after IHDR", ihdr.end()},
        {"IDAT's header only", idat.dataOffset()},
        {"IDAT's first data byte", idat.dataOffset()+1},
        {"the middle of IDAT", idat.dataOffset()+idat.length/2},
        {"IDAT's data without its CRC", idat.dataOffset()+idat.length},
        {"one byte short of IDAT's end", idat.end()-1},
    };

    for(const Cut &cut:cuts)
    {
        CAPTURE(cut.where);
        CAPTURE(cut.size);
        CHECK_FALSE(loadPrefix(png.bytes, cut.size));
    }
}

TEST_CASE("pngBuffer: every prefix that stops before the last IDAT's end returns false")
{
    SavedPng png;
    size_t idatEnd=png.idatEnd();

    REQUIRE(idatEnd>8);

    size_t loaded=0;

    for(size_t size=0; size<idatEnd; ++size)
    {
        if(loadPrefix(png.bytes, size))
        {
            CAPTURE(size);
            CHECK_MESSAGE(false, "a prefix of ", size, " of ", idatEnd, " bytes loaded");
            ++loaded;
        }
    }
    CHECK(loaded==0);
}

TEST_CASE("pngBuffer: the whole buffer loads as loadPng loads the file")
{
    SavedPng png;
    imglib::SimpleImage fromBuffer;
    imglib::SimpleImage fromFile;

    std::vector<uint8_t> exact(png.bytes);
    REQUIRE(imglib::loadPngBuffer(fromBuffer, exact.data(), exact.size()));
    REQUIRE(imglib::loadPng(fromFile, png.file.path.string().c_str()));

    CHECK(fromBuffer.format==imglib::Format::RGBA);
    CHECK(fromBuffer.depth==imglib::Depth::Bit8);
    CHECK(fromBuffer.width==40);
    CHECK(fromBuffer.height==40);
    CHECK(samePixels(fromBuffer, fromFile));
    CHECK(samePixels(fromBuffer, png.image));
}
