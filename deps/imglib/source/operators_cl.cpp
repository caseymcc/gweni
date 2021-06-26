#include "imglib/operators_cl.h"
#include "imglib/kernels_cl.h"

#include <string>
#include <cassert>

namespace imglib{namespace cl
{

//from emebeded opencl source
extern std::string operators_cl;

float sum(cl_context context, cl_command_queue commandQueue, cl_mem image)
{
//    ::cl::Buffer sumBuffer;
    cl_mem sumBuffer;

    size_t width, height;
    cl_event sumEvent;
    cl_event readEvent;

    float sumValue;

    cl_int error;

    sumBuffer=::clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_float), nullptr, &error);

	cl_image_format clFormat;
	size_t clDepth;

	clGetImageInfo(image, CL_IMAGE_FORMAT, sizeof(cl_image_format), &clFormat, nullptr);
	clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof(size_t), &clDepth, nullptr);
    clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof(size_t), &width, nullptr);
    clGetImageInfo(image, CL_IMAGE_HEIGHT, sizeof(size_t), &height, nullptr);

	Format format=Format::RGB;
	Depth depth=Depth::Bit8;

	assert(false);//need to get format and depth from above

    sum(context, commandQueue, image, width, height, format, depth, sumBuffer, 0, nullptr, &sumEvent);
    clEnqueueReadBuffer(commandQueue, sumBuffer, false, 0, sizeof(float), &sumValue, 1, &sumEvent, &readEvent);
    clWaitForEvents(1, &readEvent);

    return sumValue;
}

void sum(cl_context context, cl_command_queue commandQueue, cl_mem image, size_t width, size_t height, Format format, Depth depth, cl_mem sumBuffer, size_t eventCount, cl_event *events, cl_event *event)
{
    cl_int status;
	std::string kernelName;

	if(depth==Depth::Bit8)
		kernelName="sum_ui";
	else if(depth==Depth::Float)
		kernelName="sum_float";
	else 
		assert(false);

    cl_kernel kernel=getKernel(context, kernelName.c_str(), "operators.cl", operators_cl.c_str());
    assert(kernel);

	cl_int w=(cl_int)width;
	cl_int h=(cl_int)height;

    assert(kernel);
    status=clSetKernelArg(kernel, 0, sizeof(cl_mem), &image);
    assert(status==CL_SUCCESS);
    status=clSetKernelArg(kernel, 1, sizeof(cl_int), &w);
    assert(status==CL_SUCCESS);
    status=clSetKernelArg(kernel, 2, sizeof(cl_int), &h);
    assert(status==CL_SUCCESS);
    status=clSetKernelArg(kernel, 3, sizeof(cl_mem), &sumBuffer);
    assert(status==CL_SUCCESS);

    size_t offset[1]={0};
    size_t local[1]={64};//kernel uses 64 threads for sumation

	int globalThreads=height/64;

	if(globalThreads*64<height)
		globalThreads++;
	globalThreads=globalThreads*64;

    size_t global[1]={(size_t)globalThreads};

    status=clEnqueueNDRangeKernel(commandQueue, kernel, 1, offset, global, local, eventCount, events, event);
    assert(status==CL_SUCCESS);
}

void count(cl_context context, cl_command_queue commandQueue, cl_mem image, size_t width, size_t height, Format format, Depth depth, unsigned int value, cl_mem sumBuffer, size_t eventCount, cl_event *events, cl_event *event)
{
	cl_int status;
	std::string kernelName;

	if(depth==Depth::Bit8)
		kernelName="count_ui";
	else if(depth==Depth::Float)
//		kernelName="sum_float";
		assert(false);
	else
		assert(false);

	cl_kernel kernel=getKernel(context, kernelName.c_str(), "operators.cl", operators_cl.c_str());
	assert(kernel);

	cl_int w=(cl_int)width;
	cl_int h=(cl_int)height;

	assert(kernel);
	status=clSetKernelArg(kernel, 0, sizeof(cl_mem), &image);
	assert(status==CL_SUCCESS);
	status=clSetKernelArg(kernel, 1, sizeof(cl_int), &w);
	assert(status==CL_SUCCESS);
	status=clSetKernelArg(kernel, 2, sizeof(cl_int), &h);
	assert(status==CL_SUCCESS);
	status=clSetKernelArg(kernel, 3, sizeof(cl_uint), &value);
	assert(status==CL_SUCCESS);
	status=clSetKernelArg(kernel, 4, sizeof(cl_mem), &sumBuffer);
	assert(status==CL_SUCCESS);

	size_t offset[1]={0};
	size_t local[1]={64};//kernel uses 64 threads for sumation

	int globalThreads=height/64;

	if(globalThreads*64<height)
		globalThreads++;
	globalThreads=globalThreads*64;

	size_t global[1]={(size_t)globalThreads};

	status=clEnqueueNDRangeKernel(commandQueue, kernel, 1, offset, global, local, eventCount, events, event);
	assert(status==CL_SUCCESS);
}

}}//namespace imglib::cl