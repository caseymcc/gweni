#ifndef _imglib_probe_h_
#define _imglib_probe_h_

#include "imglib_export.h"

#include <cstddef>
#include <cstdint>

namespace imglib
{

//The encoded formats loadBuffer reads, told apart by their first bytes
enum class FileFormat
{
    Unknown,
    Png,    //the 8-byte PNG signature
    Jpeg    //SOI and the start of a marker: FF D8 FF
};

//The format the first bytes of a buffer are the signature of; Unknown for any other bytes, a buffer
//too short to hold one and a null buffer
IMGLIB_EXPORT FileFormat detectFormat(const uint8_t *buffer, size_t size);

struct ImageHeader
{
    FileFormat format=FileFormat::Unknown;
    size_t width=0;
    size_t height=0;
};

//Reads a PNG's or JPEG's width and height from its header alone, decoding nothing and allocating
//nothing: a PNG's IHDR (which must be the first chunk), a JPEG's first frame header (SOFn), found
//by walking the markers before it. False, with header.format still set where the signature was
//found, for a buffer that ends before them, a malformed marker or chunk, a zero width or height, or
//a buffer of no known format.
IMGLIB_EXPORT bool probeImage(const uint8_t *buffer, size_t size, ImageHeader &header);

}//namespace imglib

#endif //_imglib_probe_h_
