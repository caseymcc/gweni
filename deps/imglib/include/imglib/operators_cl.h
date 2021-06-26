#ifndef _imglib_operators_cl_h_
#define _imglib_operators_cl_h_

#include "imglib_export.h"
#include "imglib/image.h"

#include "CL/cl.hpp"

#include <vector>
#include <cassert>

namespace imglib{namespace cl
{

///
/// Sums values accross entire image
///
IMGLIB_EXPORT float sum(cl_context context, cl_command_queue commandQueue, cl_mem image);
template<typename _ImageType>
float sum(cl_context context, cl_command_queue commandQueue, _ImageType &image)
{
    if(location(image)!=Location::OpenCL)
    {
        assert(false);
        return 0.0f;
    }

    cl_mem native=(cl_mem)nativeId(image);

    sum(context, commandQueue, native);
}

IMGLIB_EXPORT void sum(cl_context context, cl_command_queue commandQueue, cl_mem image, size_t width, size_t height, Format format, Depth depth, cl_mem sumBuffer, size_t eventCount=0, cl_event *events=nullptr, cl_event *event=nullptr);
template<typename _ImageType>
void sum(cl_context context, cl_command_queue commandQueue, _ImageType &image, cl_mem sumBuffer, size_t eventCount=0, cl_event *events=nullptr, cl_event *event=nullptr)
{
    if(location(image)!=Location::OpenCL)
    {
        assert(false);
    }

    cl_mem native=(cl_mem)nativeId(image);

    sum(context, commandQueue, native, width(image), height(image), format(image), depth(image), sumBuffer, eventCount, events, event);
}


#ifdef IMGLIB_OPENCL_CPP

inline float sum(::cl::Context &context, ::cl::CommandQueue &commandQueue, ::cl::Image &image)
{
    return sum(context(), commandQueue(), image());
}

inline void sum(::cl::Context &context, ::cl::CommandQueue &commandQueue, ::cl::Image &image, size_t width, size_t height, Format format, Depth depth, ::cl::Buffer &sumBuffer, std::vector<::cl::Event> *events=nullptr, ::cl::Event *event=nullptr)
{
    sum(context(), commandQueue(), image(), width, height, format, depth, sumBuffer(),
        (events!=nullptr)?(cl_uint)events->size():0,
        (events!=nullptr && events->size() > 0)?(cl_event*)&events->front():nullptr,
        (event!=nullptr)?&event->operator()():nullptr);
}

template<typename _ImageType>
void sum(::cl::Context &context, ::cl::CommandQueue &commandQueue, _ImageType &image, ::cl::Buffer &sumBuffer, std::vector<::cl::Event> *events=nullptr, ::cl::Event *event=nullptr)
{
	if(location(image)!=Location::OpenCL)
	{
		assert(false);
	}

	cl_mem native=(cl_mem)nativeId(image);

	sum(context(), commandQueue(), native, width(image), height(image), format(image), depth(image), sumBuffer(),
		(events!=nullptr)?(cl_uint)events->size():0,
		(events!=nullptr && events->size()>0)?(cl_event*)&events->front():nullptr,
		(event!=nullptr)?&event->operator()():nullptr);
}
#endif//IMGLIB_OPENCL_CPP

///
/// Counts value in entire image
///
//IMGLIB_EXPORT float count(cl_context context, cl_command_queue commandQueue, cl_mem image, unsigned int value);
//template<typename _ImageType>
//float count(cl_context context, cl_command_queue commandQueue, _ImageType &image, unsigned int value)
//{
//	if(location(image)!=Location::OpenCL)
//	{
//		assert(false);
//		return 0.0f;
//	}
//
//	cl_mem native=(cl_mem)nativeId(image);
//
//	count(context, commandQueue, native, value);
//}

IMGLIB_EXPORT void count(cl_context context, cl_command_queue commandQueue, cl_mem image, size_t width, size_t height, Format format, Depth depth, unsigned int value, cl_mem sumBuffer, size_t eventCount=0, cl_event *events=nullptr, cl_event *event=nullptr);
template<typename _ImageType>
void count(cl_context context, cl_command_queue commandQueue, _ImageType &image, cl_mem sumBuffer, unsigned int value, size_t eventCount=0, cl_event *events=nullptr, cl_event *event=nullptr)
{
	if(location(image)!=Location::OpenCL)
	{
		assert(false);
	}

	cl_mem native=(cl_mem)nativeId(image);

	count(context, commandQueue, native, width(image), height(image), format(image), depth(image), value, sumBuffer, eventCount, events, event);
}


#ifdef IMGLIB_OPENCL_CPP

//inline float count(::cl::Context &context, ::cl::CommandQueue &commandQueue, ::cl::Image &image, unsigned int value)
//{
//	return count(context(), commandQueue(), image());
//}

inline void count(::cl::Context &context, ::cl::CommandQueue &commandQueue, ::cl::Image &image, size_t width, size_t height, Format format, Depth depth, unsigned int value, ::cl::Buffer &sumBuffer, std::vector<::cl::Event> *events=nullptr, ::cl::Event *event=nullptr)
{
	count(context(), commandQueue(), image(), width, height, format, depth, value, sumBuffer(),
		(events!=nullptr)?(cl_uint)events->size():0,
		(events!=nullptr && events->size()>0)?(cl_event*)&events->front():nullptr,
		(event!=nullptr)?&event->operator()():nullptr);
}

template<typename _ImageType>
void count(::cl::Context &context, ::cl::CommandQueue &commandQueue, _ImageType &image, unsigned int value, ::cl::Buffer &sumBuffer, std::vector<::cl::Event> *events=nullptr, ::cl::Event *event=nullptr)
{
	if(location(image)!=Location::OpenCL)
	{
		assert(false);
	}

	cl_mem native=(cl_mem)nativeId(image);

	count(context(), commandQueue(), native, width(image), height(image), format(image), depth(image), value, sumBuffer(),
		(events!=nullptr)?(cl_uint)events->size():0,
		(events!=nullptr && events->size()>0)?(cl_event*)&events->front():nullptr,
		(event!=nullptr)?&event->operator()():nullptr);
}

#endif//IMGLIB_OPENCL_CPP

}}//namespace imglib::cl

#endif //_imglib_operators_cl_h_