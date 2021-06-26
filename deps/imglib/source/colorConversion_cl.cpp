#include "imglib/colorConversion_cl.h"
#include "imglib/kernels_cl.h"
#include "imglib/utils_cl.h"

#include <string>
#include <cassert>

namespace imglib{namespace cl
{

size_t roundToMulitple(size_t number, size_t multiple)
{
	size_t value=number/multiple;

	if(value*multiple<number)
		value++;
	value=value*multiple;
	return value;
}

//from emebeded opencl source
extern std::string colorConversion_cl;

void rgbToRGBA(cl_context context, cl_command_queue commandQueue, cl_mem &src, size_t srcWidth, size_t srcHeight, cl_mem &dst, size_t dstWidth, size_t dstHeight, size_t eventCount, cl_event *events, cl_event *event)
{
    cl_int status;

    std::string kernalName;

    if(srcWidth%4 == 0)
        kernalName="rgbToRGBA_uint";
    else
    {
        assert(false);
        return;
    }

    cl_kernel kernel=getKernel(context, kernalName.c_str(), "colorConversion.cl", colorConversion_cl.c_str());
    assert(kernel);

    assert(kernel);
    status=clSetKernelArg(kernel, 0, sizeof(cl_mem), src);
    assert(status==CL_SUCCESS);
    status=clSetKernelArg(kernel, 1, sizeof(size_t), &dstWidth);
    assert(status==CL_SUCCESS);
    status=clSetKernelArg(kernel, 2, sizeof(size_t), &dstHeight);
    assert(status==CL_SUCCESS);
    status=clSetKernelArg(kernel, 3, sizeof(cl_mem), dst);
    assert(status==CL_SUCCESS);

    size_t offset[2]={0, 0};
    size_t local[2]={64, 64};
    size_t global[2]={roundToMulitple(dstWidth/4, local[0]), roundToMulitple(dstHeight, local[1])};

    status=clEnqueueNDRangeKernel(commandQueue, kernel, 2, offset, global, local, eventCount, events, event);
    assert(status==CL_SUCCESS);
}

void rgbToGreyScale(cl_context context, cl_command_queue commandQueue, cl_mem &src, size_t srcWidth, size_t srcHeight, Format srcFormat, Depth srcDepth,
	cl_mem &dst, size_t dstWidth, size_t dstHeight, Format dstFormat, Depth dstDepth, size_t eventCount, cl_event *events, cl_event *event)
{
	cl_int status;
	std::string kernelName;
		
	if((srcFormat==Format::RGB) ||(srcFormat==Format::RGBA))
	{
		if(srcDepth==Depth::Bit8)
			kernelName="rgb8To";
		else
			assert(false);
	}
	else
		assert(false);

	if(dstDepth==Depth::Bit8)
		kernelName+="Grey8";
	else
		assert(false);


	cl_kernel kernel=getKernel(context, kernelName.c_str(), "colorConversion.cl", colorConversion_cl.c_str());
	assert(kernel);

	int width=dstWidth;
	int height=dstHeight;

	assert(kernel);
	status=clSetKernelArg(kernel, 0, sizeof(cl_mem), &src);
	assert(status==CL_SUCCESS);
	status=clSetKernelArg(kernel, 1, sizeof(int), &width);
	assert(status==CL_SUCCESS);
	status=clSetKernelArg(kernel, 2, sizeof(int), &height);
	assert(status==CL_SUCCESS);
	status=clSetKernelArg(kernel, 3, sizeof(cl_mem), &dst);
	assert(status==CL_SUCCESS);

	size_t offset[2]={0, 0};
	size_t local[2]={32, 32};
	size_t global[2]={roundToMulitple(dstWidth, local[0]), roundToMulitple(dstHeight, local[1])};

	status=clEnqueueNDRangeKernel(commandQueue, kernel, 2, offset, global, local, eventCount, events, event);
	assert(status==CL_SUCCESS);
}

}}//namespace imglib::cl
