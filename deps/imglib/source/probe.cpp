#include "imglib/probe.h"

#include <cstring>

namespace imglib
{

namespace
{

const uint8_t pngSignature[8]={ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

uint32_t bigEndian32(const uint8_t *p)
{
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|uint32_t(p[3]);
}

uint16_t bigEndian16(const uint8_t *p)
{
    return (uint16_t)((p[0]<<8)|p[1]);
}

bool probePng(const uint8_t *buffer, size_t size, ImageHeader &header)
{
    //the signature, IHDR's length and type, and its width and height
    if(size<24)
        return false;
    if((bigEndian32(buffer+8)!=13)||(memcmp(buffer+12, "IHDR", 4)!=0))
        return false;

    const uint32_t width=bigEndian32(buffer+16);
    const uint32_t height=bigEndian32(buffer+20);

    //the PNG spec's limits: 1 to 2^31-1
    if((width==0)||(height==0)||(width>0x7fffffffu)||(height>0x7fffffffu))
        return false;
    header.width=width;
    header.height=height;
    return true;
}

//A start-of-frame marker: C0-CF but DHT (C4), JPG (C8) and DAC (CC)
bool isStartOfFrame(uint8_t marker)
{
    return (marker>=0xc0)&&(marker<=0xcf)&&(marker!=0xc4)&&(marker!=0xc8)&&(marker!=0xcc);
}

bool probeJpeg(const uint8_t *buffer, size_t size, ImageHeader &header)
{
    size_t pos=2;

    while(pos<size)
    {
        //a marker is FF and its code, any number of FF fill bytes between
        if(buffer[pos]!=0xff)
            return false;
        while((pos<size)&&(buffer[pos]==0xff))
            ++pos;
        if(pos>=size)
            return false;

        const uint8_t marker=buffer[pos++];

        //the markers with no segment: TEM and RSTn (SOI again would be malformed, but has none)
        if((marker==0x01)||((marker>=0xd0)&&(marker<=0xd8)))
            continue;
        //EOI or the scan before any frame header, or a stuffed zero where a marker should be
        if((marker==0xd9)||(marker==0xda)||(marker==0x00))
            return false;
        if(size-pos<2)
            return false;

        const size_t length=bigEndian16(buffer+pos);

        if((length<2)||(length>size-pos))
            return false;
        if(isStartOfFrame(marker))
        {
            //the length, the sample precision, then the height and the width
            if(length<7)
                return false;

            const size_t height=bigEndian16(buffer+pos+3);
            const size_t width=bigEndian16(buffer+pos+5);

            //a height of 0 is given later by a DNL marker, which is not read here
            if((width==0)||(height==0))
                return false;
            header.width=width;
            header.height=height;
            return true;
        }
        pos+=length;
    }
    return false;
}

}//namespace

FileFormat detectFormat(const uint8_t *buffer, size_t size)
{
    if(buffer==nullptr)
        return FileFormat::Unknown;
    if((size>=8)&&(memcmp(buffer, pngSignature, 8)==0))
        return FileFormat::Png;
    if((size>=3)&&(buffer[0]==0xff)&&(buffer[1]==0xd8)&&(buffer[2]==0xff))
        return FileFormat::Jpeg;
    return FileFormat::Unknown;
}

bool probeImage(const uint8_t *buffer, size_t size, ImageHeader &header)
{
    header=ImageHeader();
    header.format=detectFormat(buffer, size);

    switch(header.format)
    {
    case FileFormat::Png:
        return probePng(buffer, size, header);
    case FileFormat::Jpeg:
        return probeJpeg(buffer, size, header);
    case FileFormat::Unknown:
        break;
    }
    return false;
}

}//namespace imglib
