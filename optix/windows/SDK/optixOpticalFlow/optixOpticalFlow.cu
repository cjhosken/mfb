//
// Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//


#include <optix.h>
#include "optix_denoiser_opticalflow.h"


static inline unsigned int divUp( unsigned int nominator, unsigned int denominator )
{
    return ( nominator + denominator - 1 ) / denominator;
}

struct floatRdAccess
{
    inline floatRdAccess( const OptixImage2D& im )
        : image( im )
        , psb( im.pixelStrideInBytes )
        , hf( image.format == OPTIX_PIXEL_FORMAT_HALF2 || image.format == OPTIX_PIXEL_FORMAT_HALF3 || image.format == OPTIX_PIXEL_FORMAT_HALF4 )
    {
        if( im.pixelStrideInBytes == 0 )
        {
            unsigned int dsize = hf ? sizeof( __half ) : sizeof( float );
            psb                = getNumChannels( im ) * dsize;
        }
    }
    inline __device__ float read( int x, int y, int c ) const
    {
        if( hf )
            return float( *(const __half*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( __half ) ) );
        else
            return float( *(const float*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( float ) ) );
    }
    OptixImage2D image;
    unsigned int psb;
    bool         hf;
};

struct floatWrAccess
{
    inline floatWrAccess( const OptixImage2D& im )
        : image( im )
        , psb( im.pixelStrideInBytes )
        , hf( image.format == OPTIX_PIXEL_FORMAT_HALF2 || image.format == OPTIX_PIXEL_FORMAT_HALF3 || image.format == OPTIX_PIXEL_FORMAT_HALF4 )
    {
        if( im.pixelStrideInBytes == 0 )
        {
            unsigned int dsize = hf ? sizeof( __half ) : sizeof( float );
            psb                = getNumChannels( im ) * dsize;
        }
    }
    inline __device__ void write( int x, int y, int c, float value )
    {
        if( hf )
            *(__half*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( __half ) ) = value;
        else
            *(float*)( image.data + y * image.rowStrideInBytes + x * psb + c * sizeof( float ) ) = value;
    }
    OptixImage2D image;
    unsigned int psb;
    bool         hf;
};

static __global__ void k_convertRGBA( unsigned char* result, floatRdAccess input, int outStrideX )
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if( x >= input.image.width || y >= input.image.height )
        return;

    unsigned int r = __saturatef( input.read( x, y, 0 ) ) * 255.f;
    unsigned int g = __saturatef( input.read( x, y, 1 ) ) * 255.f;
    unsigned int b = __saturatef( input.read( x, y, 2 ) ) * 255.f;

    *(unsigned int*)&result[y * outStrideX + x * 4] = b | ( g << 8 ) | ( r << 16 ) | ( 255u << 24 );
}

static __global__ void k_convertFlow( floatWrAccess result, const int16_t* input, int inStrideX )
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if( x >= result.image.width || y >= result.image.height )
        return;

    result.write( x, y, 0, float( input[y * inStrideX + x * 2 + 0] ) * ( 1.f / 32.f ) );
    result.write( x, y, 1, float( input[y * inStrideX + x * 2 + 1] ) * ( 1.f / 32.f ) );
}

OptixResult convertRGBA( unsigned char* result, const OptixImage2D& input, uint32_t inStrideXInBytes, CUstream stream )
{
        dim3 block( 32, 32, 1 );
        dim3 grid = dim3( divUp( input.width, block.x ), divUp( input.height, block.y ), 1 );

        k_convertRGBA<<<grid, block, 0, stream>>>( result, floatRdAccess( input ), inStrideXInBytes );

        return OPTIX_SUCCESS;
}

OptixResult convertFlow( OptixImage2D& result, const int16_t* flow, uint32_t outStrideXInBytes, CUstream stream )
{
        dim3 block( 32, 32, 1 );
        dim3 grid = dim3( divUp( result.width, block.x ), divUp( result.height, block.y ), 1 );

        // convert 2x16 bit fixpoint to 2xfp16/2xfp32 bit flow vectors
        k_convertFlow<<<grid, block, 0, stream>>>( floatWrAccess( result ), flow, outStrideXInBytes );

        return OPTIX_SUCCESS;
}

extern OptixResult runOpticalFlow( CUcontext ctx, CUstream stream, OptixImage2D & flow, OptixImage2D input[2], float & flowTime, std::string & errMessage )
{
    OptixUtilOpticalFlow oflow;

    if( const OptixResult res = oflow.init( ctx, stream, input[0].width, input[0].height ) )
    {
        errMessage = oflow.getLastError();
        return res;
    }

    CUevent start, stop;
    cuEventCreate( &start, 0 );
    cuEventCreate( &stop, 0 );
    cuEventRecord( start, stream );
    
    if( const OptixResult res = oflow.computeFlow( flow, input ) )
    {
        errMessage = oflow.getLastError();
        cuEventDestroy( start );
        cuEventDestroy( stop );
        return res;
    }

    cuEventRecord(stop, stream);
    cuEventSynchronize( stop );
    cuEventElapsedTime(&flowTime, start, stop);

    cuEventDestroy( start );
    cuEventDestroy( stop );

    return oflow.destroy();
}
