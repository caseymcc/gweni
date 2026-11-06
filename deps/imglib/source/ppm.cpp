#include "imglib/ppm.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <limits>

namespace imglib
{

void writePpmGreyScale(ImageWrapper &image, FILE *file);
void writePpmGreyScale32f(ImageWrapper &image, FILE *file);
void writePpmRGB(ImageWrapper &image, FILE *file);
void writePpmRGBA(ImageWrapper &image, FILE *file);

bool isPpmWhitespace(char c)
{
    return (c=='\n')||(c=='\r')||(c==' ')||(c=='\t');
}

//reads one unsigned decimal header field and the single whitespace byte that ends it; fails on an
//empty, signed, non-numeric or too long (fills the buffer) token
bool readNextNumber(FILE *file, std::vector<char> &buffer, size_t &number)
{
    //skip whitespace
    size_t read=fread(buffer.data(), 1, 1, file);
    while((read!=0)&&isPpmWhitespace(buffer[0]))
    {
        read=fread(buffer.data(), 1, 1, file);
    }

    if(read==0)
        return false;

    //read number, keeping the last byte of the buffer for the terminator
    size_t index=0;

    while(!isPpmWhitespace(buffer[index]))
    {
        if(index+1>=buffer.size())
            return false;

        index++;
        if(fread(&buffer[index], 1, 1, file)!=1)
            return false;
    }

    buffer[index]='\0';

    //strtoull skips leading whitespace and takes a sign, so require a digit first
    if((buffer[0]<'0')||(buffer[0]>'9'))
        return false;

    char *end=nullptr;
    unsigned long long value=strtoull(buffer.data(), &end, 10);

    if((end!=&buffer[index])||(value>std::numeric_limits<size_t>::max()))
        return false;

    number=(size_t)value;
    return true;
}

//bytes left in the file from the current position, false if the file can not be seeked
bool remainingBytes(FILE *file, size_t &remaining)
{
    long position=ftell(file);

    if((position<0)||(fseek(file, 0, SEEK_END)!=0))
        return false;

    long size=ftell(file);

    if((size<position)||(fseek(file, position, SEEK_SET)!=0))
        return false;

    remaining=(size_t)(size-position);
    return true;
}

bool readPpm(ImageWrapper &image, FILE *file)
{
    std::vector<char> buffer(10);

    if(fread(buffer.data(), 1, 2, file)!=2)
        return false;

    if(strncmp(buffer.data(), "P6", 2)!=0)
        return false;

    size_t width;
    size_t height;
    size_t maxColor;

    if(!readNextNumber(file, buffer, width))
        return false;
    if(!readNextNumber(file, buffer, height))
        return false;
    if(!readNextNumber(file, buffer, maxColor))
        return false;

    //only 8 bit samples are supported
    if(maxColor!=255)
        return false;

    if((width==0)||(height==0)||(width>SIZE_MAX/3/height))
        return false;

    size_t pixelBytes=width*height*3;

    //reject a header that promises more pixels than the file holds before allocating for them
    size_t remaining;

    if(remainingBytes(file, remaining)&&(remaining<pixelBytes))
        return false;

    image.resize(Format::RGB, Depth::Bit8, width, height);

    if(image.dataSize()<pixelBytes)
        return false;

    return fread(image.data(), 1, pixelBytes, file)==pixelBytes;
}

bool loadPpm(ImageWrapper image, const char *filename)
{
    FILE *file=fopen(filename, "rb");

    if(file==NULL)
        return false;

    bool success=readPpm(image, file);

    fclose(file);
    return success;
}

bool savePpm(ImageWrapper image, const char *filename)
{
	FILE *file=fopen(filename, "wb");

	if(file==NULL)
		return false;

    bool success=false;
	fprintf(file, "P6\n%d %d\n255\n", (int)image.width(), (int)image.height());

	switch(image.format())
	{
	case Format::Binary:
	case Format::GreyScale:
        if(image.depth()==Depth::Bit8)
        {
            writePpmGreyScale(image, file);
            success=true;
        }
        else if(image.depth()==Depth::Float)
        {
            writePpmGreyScale32f(image, file);
            success=true;
        }
		break;
	case Format::RGB:
        if(image.depth()==Depth::Bit8)
        {
            writePpmRGB(image, file);
            success=true;
        }
		break;
	case Format::RGBA:
        if(image.depth()==Depth::Bit8)
        {
            writePpmRGBA(image, file);
            success=true;
        }
		break;
	case Format::Unknown:
	case Format::RA:
	case Format::BGRA:
	case Format::YUV:
		//no P6 ppm encoding for these, success stays false
		break;
	}
	
	fclose(file);
	return success;
}

void writePpmGreyScale(ImageWrapper &image, FILE *file)
{
	uint8_t *data=image.data();

	for(size_t y=0; y<image.height(); ++y)
	{
		for(size_t x=0; x<image.width(); ++x)
		{
			fwrite(data, 1, 1, file);
			fwrite(data, 1, 1, file);
			fwrite(data, 1, 1, file);
			++data;
		}
	}
}

void writePpmGreyScale32f(ImageWrapper &image, FILE *file)
{
	float *data=(float *)image.data();

	float min=std::numeric_limits<float>::max();
	float max=std::numeric_limits<float>::min();

	for(size_t y=0; y<image.height(); ++y)
	{
		for(size_t x=0; x<image.width(); ++x)
		{
			float &value=*data;

			if(value>max)
				max=value;
			else if(value<min)
				min=value;
			++data;
		}
	}
	
	float range=max-min;
	float scale=255.0f/range;

	data=(float *)image.data();
	for(size_t y=0; y<image.height(); ++y)
	{
		for(size_t x=0; x<image.width(); ++x)
		{
			uint8_t value=((*data)+min)*scale;
			fwrite(&value, 1, 1, file);
			fwrite(&value, 1, 1, file);
			fwrite(&value, 1, 1, file);
			++data;
		}
	}
}

void writePpmRGB(ImageWrapper &image, FILE *file)
{
	uint8_t *data=image.data();

	for(size_t y=0; y<image.height(); ++y)
	{
		for(size_t x=0; x<image.width(); ++x)
		{
			fwrite(data, 1, 3, file);
			data+=3;
		}
	}
}

void writePpmRGBA(ImageWrapper &image, FILE *file)
{
	uint8_t *data=image.data();

	for(size_t y=0; y<image.height(); ++y)
	{
		for(size_t x=0; x<image.width(); ++x)
		{
			fwrite(data, 1, 3, file);
			data+=4;
		}
	}
}

}//namespace imglib