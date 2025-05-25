/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/// @file
/// @author NVIDIA Corporation
/// @brief  OptiX public API header

#ifndef optix_denoiser_opticalflow_h
#define optix_denoiser_opticalflow_h

#ifndef _WIN32
#include <dlfcn.h>
#else
#define no_init_all deprecated
#include <windows.h>
#endif

#include <optix.h>

#include "nvOpticalFlowCommon.h"
#include "nvOpticalFlowCuda.h"
#include <cuda.h>
#include <cuda_fp16.h>
#include <string>

extern OptixResult convertRGBA( unsigned char* result, const OptixImage2D& input, uint32_t inStrideXInBytes, CUstream stream );
extern OptixResult convertFlow( OptixImage2D& result, const int16_t* flow, uint32_t outStrideXInBytes, CUstream stream );
extern OptixResult runOpticalFlow( CUcontext ctx, CUstream stream, OptixImage2D & flow, OptixImage2D input[2], float & flowTime, std::string & errMessage );

static inline unsigned int getNumChannels( const OptixImage2D& image )
{
    switch( image.format )
    {
        // obsolete formats - not supported
        case OPTIX_PIXEL_FORMAT_UCHAR3:
            return 3;
        case OPTIX_PIXEL_FORMAT_UCHAR4:
            return 4;

        case OPTIX_PIXEL_FORMAT_HALF2:
        case OPTIX_PIXEL_FORMAT_FLOAT2:
            return 2;
        case OPTIX_PIXEL_FORMAT_HALF3:
        case OPTIX_PIXEL_FORMAT_FLOAT3:
            return 3;
        case OPTIX_PIXEL_FORMAT_HALF4:
        case OPTIX_PIXEL_FORMAT_FLOAT4:
            return 4;

        // internal format, not used in this sample
        case OPTIX_PIXEL_FORMAT_INTERNAL_GUIDE_LAYER:
            return 0;
    }
    return 0;
}

class OptixUtilOpticalFlow
{
  public:
    /// Constructor
    OptixUtilOpticalFlow()
        : m_ofh( nullptr )
        , m_gpuBufferOut( nullptr )
    {
        m_gpuBufferIn[0] = nullptr;
        m_gpuBufferIn[1] = nullptr;
        m_hModule        = nullptr;
    }

    /// Destructor
    ~OptixUtilOpticalFlow(){};

    /// Initialize optical flow class
    ///
    /// \param[in] ctx       the device context
    /// \param[in] stream    the stream used for operations in this class
    /// \param[in] width     width of images passed to this class
    /// \param[in] height    height of images passed to this class
    OptixResult init( CUcontext ctx, CUstream stream, unsigned int width, unsigned int height )
    {
        m_stream = stream;
        m_width  = width;
        m_height = height;

        typedef NV_OF_STATUS( NVOFAPI * PFNNvOFAPICreateInstanceCuda )( uint32_t apiVer, NV_OF_CUDA_API_FUNCTION_LIST * cudaOf );
#if defined( _WIN32 )
        m_hModule = LoadLibrary( TEXT( "nvofapi64.dll" ) );
        PFNNvOFAPICreateInstanceCuda NvOFAPICreateInstanceCuda =
            (PFNNvOFAPICreateInstanceCuda)GetProcAddress( m_hModule, "NvOFAPICreateInstanceCuda" );
#else
        m_hModule = dlopen( "libnvidia-opticalflow.so.1", RTLD_LAZY );
        PFNNvOFAPICreateInstanceCuda NvOFAPICreateInstanceCuda =
            (PFNNvOFAPICreateInstanceCuda)dlsym( m_hModule, "NvOFAPICreateInstanceCuda" );
#endif
        if( !NvOFAPICreateInstanceCuda )
            return OPTIX_ERROR_INTERNAL_ERROR;

        if( NvOFAPICreateInstanceCuda( NV_OF_API_VERSION, &m_ofl ) != NV_OF_SUCCESS )
            return OPTIX_ERROR_INTERNAL_ERROR;

        if( m_ofl.nvCreateOpticalFlowCuda( ctx, &m_ofh ) != NV_OF_SUCCESS )
            return OPTIX_ERROR_INTERNAL_ERROR;

        m_ofl.nvOFSetIOCudaStreams( m_ofh, stream, stream );

        NV_OF_INIT_PARAMS ipa   = {};
        ipa.width               = m_width;
        ipa.height              = m_height;
        ipa.enableExternalHints = NV_OF_FALSE;
        ipa.enableOutputCost    = NV_OF_FALSE;
        ipa.hintGridSize        = NV_OF_HINT_VECTOR_GRID_SIZE_UNDEFINED;
        ipa.outGridSize         = NV_OF_OUTPUT_VECTOR_GRID_SIZE_1;
        ipa.mode                = NV_OF_MODE_OPTICALFLOW;
        ipa.perfLevel           = NV_OF_PERF_LEVEL_SLOW;
        ipa.enableRoi           = NV_OF_FALSE;

        if( m_ofl.nvOFInit( m_ofh, &ipa ) != NV_OF_SUCCESS )
            return OPTIX_ERROR_INTERNAL_ERROR;

        NV_OF_BUFFER_DESCRIPTOR inputBufferDesc = {};
        inputBufferDesc.width                   = m_width;
        inputBufferDesc.height                  = m_height;
        inputBufferDesc.bufferFormat            = NV_OF_BUFFER_FORMAT_ABGR8;
        inputBufferDesc.bufferUsage             = NV_OF_BUFFER_USAGE_INPUT;

        for( int i = 0; i < 2; i++ )
        {
            if( m_ofl.nvOFCreateGPUBufferCuda( m_ofh, &inputBufferDesc, NV_OF_CUDA_BUFFER_TYPE_CUDEVICEPTR, &m_gpuBufferIn[i] ) != NV_OF_SUCCESS )
                return OPTIX_ERROR_INTERNAL_ERROR;
            m_devPtr[i] = m_ofl.nvOFGPUBufferGetCUdeviceptr( m_gpuBufferIn[i] );

            NV_OF_CUDA_BUFFER_STRIDE_INFO strideInfo;
            if( m_ofl.nvOFGPUBufferGetStrideInfo( m_gpuBufferIn[i], &strideInfo ) != NV_OF_SUCCESS )
                return OPTIX_ERROR_INTERNAL_ERROR;
            m_inStrideXInBytes = strideInfo.strideInfo[0].strideXInBytes;
        }

        NV_OF_BUFFER_DESCRIPTOR outputBufferDesc = {};
        outputBufferDesc.width                   = m_width;
        outputBufferDesc.height                  = m_height;
        outputBufferDesc.bufferFormat            = NV_OF_BUFFER_FORMAT_SHORT2;
        outputBufferDesc.bufferUsage             = NV_OF_BUFFER_USAGE_OUTPUT;

        if( m_ofl.nvOFCreateGPUBufferCuda( m_ofh, &outputBufferDesc, NV_OF_CUDA_BUFFER_TYPE_CUDEVICEPTR, &m_gpuBufferOut ) != NV_OF_SUCCESS )
            return OPTIX_ERROR_INTERNAL_ERROR;

        m_devPtrOut = m_ofl.nvOFGPUBufferGetCUdeviceptr( m_gpuBufferOut );
        NV_OF_CUDA_BUFFER_STRIDE_INFO strideInfo;
        if( m_ofl.nvOFGPUBufferGetStrideInfo( m_gpuBufferOut, &strideInfo ) != NV_OF_SUCCESS )
            return OPTIX_ERROR_INTERNAL_ERROR;
        m_outStrideXInBytes = strideInfo.strideInfo[0].strideXInBytes;

        return OPTIX_SUCCESS;
    }

    /// Destroy resources created by optical flow class
    OptixResult destroy()
    {
        if( m_gpuBufferIn[0] )
            m_ofl.nvOFDestroyGPUBufferCuda( m_gpuBufferIn[0] );
        if( m_gpuBufferIn[1] )
            m_ofl.nvOFDestroyGPUBufferCuda( m_gpuBufferIn[1] );
        if( m_gpuBufferOut )
            m_ofl.nvOFDestroyGPUBufferCuda( m_gpuBufferOut );
        if( m_ofh )
            m_ofl.nvOFDestroy( m_ofh );
        if( m_hModule )
        {
#if defined( _WIN32 )
            FreeLibrary( m_hModule );
#else
            dlclose( m_hModule );
#endif
        }
        m_ofh = nullptr;
        return OPTIX_SUCCESS;
    }

    /// Calculate optical flow between two given images
    /// \param[out] flow      returned flow vectors for each pixel
    /// \param[in] input      array of two images
    OptixResult computeFlow( OptixImage2D& flow, const OptixImage2D* input )
    {
        // convert float/fp16 RGB input to 4x8 bit ABGR
        if( OptixResult res = convertRGBA( (unsigned char*)m_devPtr[0], input[0], m_inStrideXInBytes, m_stream ) )
            return res;
        if( OptixResult res = convertRGBA( (unsigned char*)m_devPtr[1], input[1], m_inStrideXInBytes, m_stream ) )
            return res;

        NV_OF_EXECUTE_INPUT_PARAMS execInParams = {};
        execInParams.inputFrame                 = m_gpuBufferIn[0];
        execInParams.referenceFrame             = m_gpuBufferIn[1];
        execInParams.disableTemporalHints       = NV_OF_FALSE;

        NV_OF_EXECUTE_OUTPUT_PARAMS execOutParams = {};
        execOutParams.outputBuffer                = m_gpuBufferOut;

        if( m_ofl.nvOFExecute( m_ofh, &execInParams, &execOutParams ) != NV_OF_SUCCESS )
            return OPTIX_ERROR_INTERNAL_ERROR;

        if( flow.width != m_width || flow.height != m_height || getNumChannels( flow ) == 0 )
            return OPTIX_ERROR_INVALID_VALUE;

        // convert 2x16 bit fixpoint to 2xfp16/2xfp32 bit flow vectors
        if( OptixResult res = convertFlow( flow, (const int16_t*)m_devPtrOut, m_outStrideXInBytes / sizeof( short ), m_stream ) )
            return res;

        return OPTIX_SUCCESS;
    }

    std::string getLastError()
    {
        std::string message;
        if( m_ofh == nullptr )
            return std::string( "Class not initialized" );
        char lastError[MIN_ERROR_STRING_SIZE];
        uint32_t eSize = MIN_ERROR_STRING_SIZE;
        m_ofl.nvOFGetLastError( m_ofh, lastError, &eSize );
        return std::string( lastError );
    }

  private:
    unsigned int                 m_width;
    unsigned int                 m_height;
    CUstream                     m_stream;
    unsigned int                 m_inStrideXInBytes;
    unsigned int                 m_outStrideXInBytes;
    NV_OF_CUDA_API_FUNCTION_LIST m_ofl;
    NvOFHandle                   m_ofh;
    NvOFGPUBufferHandle          m_gpuBufferIn[2];
    NvOFGPUBufferHandle          m_gpuBufferOut;
    CUdeviceptr                  m_devPtr[2];
    CUdeviceptr                  m_devPtrOut;
#if defined( _WIN32 )
    HMODULE m_hModule;
#else
    void* m_hModule;
#endif
};

#endif
