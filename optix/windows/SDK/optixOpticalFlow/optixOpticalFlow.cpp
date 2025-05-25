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
#include <sutil/Exception.h>
#include <sutil/sutil.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "optix_denoiser_opticalflow.h"

//------------------------------------------------------------------------------
//
//  optixOpticalFlow -- Demonstration of OptiX optical flow
//
//------------------------------------------------------------------------------

// filename is copied to result and the first sequence of "+" characters is
// replaced (using leading zeros) with framename.
// true is returned if the framenumber is -1 or if the function was successful.

static bool getFrameFilename( std::string& result, const std::string& filename, int frame )
{
    result = filename;
    if( frame == -1 )
        return true;
    size_t nplus = 0;
    size_t ppos  = result.find( '+' );
    if( ppos == std::string::npos )
        return true;  // static filename without "+" characters
    size_t cpos = ppos;
    while( result[cpos] != 0 && result[cpos] == '+' )
    {
        nplus++;
        cpos++;
    }
    std::string fn = std::to_string( frame );
    if( fn.length() > nplus )
    {
        std::cerr << "illegal temporal filename, framenumber requires " << fn.length()
                  << " digits, \"+\" placeholder length: " << nplus << "too small" << std::endl;
        return false;
    }
    for( size_t i = 0; i < nplus; i++ )
        result[ppos + i] = '0';
    for( size_t i = 0; i < fn.length(); i++ )
        result[ppos + nplus - 1 - i] = fn[fn.length() - 1 - i];
    return true;
}

void printUsageAndExit( const std::string& argv0 )
{
    std::cerr << "Usage: " << argv0 << " [-o flow.exr] [-F | --Frames <int-int>] frame1.exr [frame2.exr]\n";
    std::cerr << "Input image and flow output filenames could have '+' in the name, which is replaced by the frame number.\n";
    std::cerr << "Calculates flow vectors from frame1 to frame2\n";
    exit( 1 );
}

// Create float OptixImage2D with given dimension and channel count. Allocate memory on device.

static OptixImage2D createOptixImage2D( unsigned int width, unsigned int height, unsigned int nChannels )
{
    OptixImage2D oi = {};

    const uint64_t frame_byte_size = width * height * nChannels * sizeof( float );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &oi.data ), frame_byte_size ) );
    oi.width              = width;
    oi.height             = height;
    oi.rowStrideInBytes   = width * nChannels * sizeof( float );
    oi.pixelStrideInBytes = nChannels * sizeof( float );
    oi.format = nChannels == 2 ? OPTIX_PIXEL_FORMAT_FLOAT2 : nChannels == 3 ? OPTIX_PIXEL_FORMAT_FLOAT3 : OPTIX_PIXEL_FORMAT_FLOAT4;
    return oi;
}

// Copy host memory to device memory

static void initOptixImage2D( OptixImage2D& result, const float* hmem )
{
    const uint64_t frame_byte_size = result.height * result.rowStrideInBytes;
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( result.data ), hmem, frame_byte_size, cudaMemcpyHostToDevice ) );
}

int32_t main( int32_t argc, char** argv )
{
    int firstFrame = -1, lastFrame = -1;
    
    std::string outputFilename;
    std::string inputFilename1;
    std::string inputFilename2;

    if( argc < 3 )                              // minimum: two image filenames
        printUsageAndExit( argv[0] );

    for( int32_t i = 1; i < argc; ++i )
    {
        std::string arg( argv[i] );
        
        if( arg == "-o" || arg == "--out" )
        {
            if( i == argc - 2 )
                printUsageAndExit( argv[0] );
            outputFilename = argv[++i];
        }
        else if( arg == "-F" || arg == "--Frames" )
        {
            if( i == argc - 2 )
                printUsageAndExit( argv[0] );
            std::string s( argv[++i] );
            size_t      cpos = s.find( '-' );
            if( cpos == 0 || cpos == s.length() - 1 || cpos == std::string::npos )
                printUsageAndExit( argv[0] );
            firstFrame = atoi( s.substr( 0, cpos ).c_str() );
            lastFrame  = atoi( s.substr( cpos + 1 ).c_str() );
            if( firstFrame < 0 || lastFrame < 0 || firstFrame > lastFrame )
            {
                std::cerr << "illegal frame range, first frame must be <= last frame and >= 0" << std::endl;
                exit( 0 );
            }
        }
        else
        {
            if( inputFilename1.empty() )
                inputFilename1 = arg;
            else if( inputFilename2.empty() )
                inputFilename2 = arg;
            else
                printUsageAndExit( argv[0] ); 
        }
    }

    if( firstFrame >= 0 && ( firstFrame == lastFrame ) )
    {
        std::cerr << "Last frame number must be greater than first frame number.";
        return 1;
    }

    try
    {
        CUcontext cuCtx = 0;
        CUDA_CHECK( (cudaError_t)cuInit( 0 ) );
        CUDA_CHECK( (cudaError_t)cuCtxCreate( &cuCtx, 0, 0 ) );
        CUstream stream = 0;

        OptixUtilOpticalFlow oflow;

        std::string frameFilename;
        if( !getFrameFilename( frameFilename, inputFilename1, firstFrame ) )
            return 1;

        sutil::ImageBuffer frame0 = sutil::loadImage( frameFilename.c_str() );
        std::cout << "Optical flow with resolution " << frame0.width << " x " << frame0.height << std::endl;
        std::cout << "Loaded " << frameFilename << std::endl;

        unsigned int width  = frame0.width;
        unsigned int height = frame0.height;

        OptixImage2D images[2] = { createOptixImage2D( width, height, frame0.pixel_format == sutil::FLOAT4 ? 4 : 3 ),
                                   createOptixImage2D( width, height, frame0.pixel_format == sutil::FLOAT4 ? 4 : 3 ) };

        initOptixImage2D( images[0], (const float*)frame0.data );
        frame0.destroy();

        if( const OptixResult res = oflow.init( cuCtx, stream, width, height ) )
        {
            std::cerr << "Initialization of optical flow failed: " << oflow.getLastError() << "\n";
            return 1;
        }

        // We could create a 2-channel format for flow, but sutil::ImageBuffer does not support this format.
        // The optical flow implementation will leave the third channel as-is and write only the first two.
        // A fp16 format would be sufficient for the flow vectors, for simplicity we use fp32 here.
        OptixImage2D flow = createOptixImage2D( width, height, 3 );

        void * hflow;
        CUDA_CHECK( (cudaError_t)cuMemAllocHost( &hflow, images[0].rowStrideInBytes * height ) );

        if( lastFrame == -1 )
        {
            lastFrame = 0;
            frameFilename = inputFilename2;
        }

        for( int frame = firstFrame; frame < lastFrame; frame++ )
        {
            if( frame != -1 && !getFrameFilename( frameFilename, inputFilename1, frame+1 ) )
                return 1;

            sutil::ImageBuffer frame1 = sutil::loadImage( frameFilename.c_str() );
            std::cout << "Loaded " << frameFilename << std::endl;

            if( frame1.width != width || frame1.height != height )
            {
                std::cerr << "Input files must have the same resolution" << std::endl;
                return 1;
            }
            if( !( frame1.pixel_format == sutil::FLOAT3 || frame1.pixel_format == sutil::FLOAT4 ) )
            {
                std::cerr << "Input files must have three or four channels" << std::endl;
                return 1;
            }
            initOptixImage2D( images[1], (const float*)frame1.data );

            OptixResult res = oflow.computeFlow( flow, images );
            
            if( res != OPTIX_SUCCESS )
            {
                std::cerr << "Error in flow calculation: " << oflow.getLastError() << std::endl;
                return 1;
            }

            initOptixImage2D( images[0], (const float*)frame1.data );
            frame1.destroy();

            CUDA_CHECK( (cudaError_t)cuMemcpyDtoHAsync( hflow, flow.data, flow.rowStrideInBytes * flow.height, stream ) );
            CUDA_CHECK( (cudaError_t)cuStreamSynchronize( stream ) );

            sutil::ImageBuffer flowImage = {};
            flowImage.width              = width;
            flowImage.height             = height;
            flowImage.data               = hflow;
            flowImage.pixel_format       = sutil::FLOAT3;

            if( !getFrameFilename( frameFilename, outputFilename, frame+1 ) )
                return 1;

            if( !frameFilename.empty() )
            {
                sutil::saveImage( frameFilename.c_str(), flowImage, false );
                std::cout << "Wrote " << frameFilename << std::endl;
            }
        }
        CUDA_CHECK( (cudaError_t)cuMemFreeHost( hflow ) );

        oflow.destroy();
    }
    catch( std::exception& e )
    {
        std::cerr << "ERROR: exception caught '" << e.what() << "'" << std::endl;
    }
    return 0;
}
