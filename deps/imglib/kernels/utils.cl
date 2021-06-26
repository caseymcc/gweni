__kernel void zeroMemory(__global uchar *buffer)
{
    const int globalX=get_global_id(0);

    buffer[globalX]=0;
}

__kernel void zeroFloatMemory(__global float *buffer)
{
    const int globalX=get_global_id(0);

    buffer[globalX]=0.0f;
}

__kernel void zeroIntMemory(__global int *buffer)
{
    const int globalX=get_global_id(0);

    buffer[globalX]=0.0f;
}

__kernel void copyToFloat(__global uint *fromBuffer, uint fromOffset, __global float *toBuffer, uint toOffset, uint size)
{
	const uint index=get_global_id(0);

	toBuffer[index+toOffset]=(float)(fromBuffer[index+fromOffset]);
}