#pragma OPENCL EXTENSION cl_khr_int64_base_atomics: enable

#include "atomic.cl"

__kernel void sum_ui(read_only image2d_t input, int width, int height, __global ulong *sumBuffer)
{
	const sampler_t nearestClampSampler=CLK_NORMALIZED_COORDS_FALSE|CLK_FILTER_NEAREST|CLK_ADDRESS_CLAMP_TO_EDGE;
	const int y=get_global_id(0);
	const int localId=get_local_id(0);
	__local uint localSum[64];

	if(y==0)
		(*sumBuffer)=0;

	if(y>=height)
		return;

	uint lineSum=0;
	int2 pos=(int2)(0, y);

	for(int x=0; x<width; x++)
	{
		pos.x=x;
		lineSum+=read_imageui(input, nearestClampSampler, pos).x;
	}

	localSum[localId]=lineSum;

	//sumBuffer(global) set by y==0, localSum(local) set by everything else
	barrier(CLK_LOCAL_MEM_FENCE|CLK_GLOBAL_MEM_FENCE);

	if(localId!=0)
		return;

	const int groupId=get_group_id(0);
	int count=64;
	const int index=groupId*count;

	if(index+64>height)
		count=height-index;

	ulong threadSum=localSum[0];
	for(int i=1; i<count; i++)
		threadSum+=localSum[i];

	atomic_add(sumBuffer, threadSum);
}

__kernel void sum_float(read_only image2d_t input, int width, int height, __global float *sumBuffer)
{
    const sampler_t nearestClampSampler=CLK_NORMALIZED_COORDS_FALSE|CLK_FILTER_NEAREST|CLK_ADDRESS_CLAMP_TO_EDGE;
    const int y=get_global_id(0);
    const int localId=get_local_id(0);
    __local float localSum[64];

    if(y==0)
        (*sumBuffer)=0;

	if(y>=height)
		return;

	float lineSum=0;
    int2 pos=(int2)(0, y);

    for(int x=0; x<width; x++)
    {
        pos.x=x;
		lineSum+=read_imagef(input, nearestClampSampler, pos).x;
    }

    localSum[localId]=lineSum;
    
    //sumBuffer(global) set by y==0, localSum(local) set by everything else
    barrier(CLK_LOCAL_MEM_FENCE|CLK_GLOBAL_MEM_FENCE);

    if(localId!=0)
        return;

    const int groupId=get_group_id(0);
    int count=64;
    const int index=groupId*count;

    if(index+64>height)
        count=height-index;

    float threadSum=localSum[0];
    for(int i=1; i<count; i++)
		threadSum+=localSum[i];

    atomicAddFloat_Global(sumBuffer, threadSum);
}

__kernel void count_ui(read_only image2d_t input, int width, int height, uint value, __global uint *sumBuffer)
{
	const sampler_t nearestClampSampler=CLK_NORMALIZED_COORDS_FALSE|CLK_FILTER_NEAREST|CLK_ADDRESS_CLAMP_TO_EDGE;
	const int y=get_global_id(0);
	const int localId=get_local_id(0);
	__local uint localSum[64];

	if(y==0)
		(*sumBuffer)=0;

	if(y>=height-1)
		return;

	uint lineSum=0;
	int2 pos=(int2)(0, y);

	for(int x=0; x<width-1; x++)
	{
		pos.x=x;
		uint imageValue=read_imageui(input, nearestClampSampler, pos).x;

		if(imageValue == value)
			lineSum++;
	}
	localSum[localId]=lineSum;

	//sumBuffer(global) set by y==0, localSum(local) set by everything else
	barrier(CLK_LOCAL_MEM_FENCE|CLK_GLOBAL_MEM_FENCE);

	if(localId!=0)
		return;

	int count=64;
	const int index=y*count;

	if(index+64>height)
		count=height-index;

	uint threadSum=localSum[0];
	for(int i=1; i<count; i++)
		threadSum+=localSum[i];

	atomic_add(sumBuffer, threadSum);
//	if(y==0)
//		(*sumBuffer)=lineSum;
}
