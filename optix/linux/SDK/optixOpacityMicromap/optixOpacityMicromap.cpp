//
// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

//-----------------------------------------------------------------------------
//
// A simple demonstration of opacity micromaps.  
//
// * A single quad, made of two triangles ABC and ACD is rendered with a 
//   transparent circular cutout at its center.  
// * OMMs are applied to the two triangles to accelerate the evaluation of the
//   opacity function during traversal.
// * As a preproces, OMM microtriangles are marked as either completely 
//   transparent, completely opaque, or unknown.
// * During traversal, rays that hit opaque or transparent regions of the OMM
//   can skip the anyhit function.
// * Rays that hit 'unknown' regions of the OMM evaluate the anyhit to get
//   accurate evaluation of the opacity function.
// * Regions of the micromap which are unknown are tinted a lighter color to
//   visualize the regions which required anyhit evaluation.
//
//-----------------------------------------------------------------------------

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_micromap.h>
#include <optix_stack_size.h>
#include <optix_stubs.h>

#include <cuda_runtime.h>

#include <sampleConfig.h>

#include <sutil/CUDAOutputBuffer.h>
#include <sutil/Exception.h>
#include <sutil/sutil.h>

#include "optixOpacityMicromap.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <string>

#include <sutil/Camera.h>
#include <sutil/Trackball.h>


constexpr int OMM_SUBDIV_LEVEL = 4;
constexpr int NUM_TRIS         = 2; 
constexpr int DEFAULT_WIDTH    = 1024;
constexpr int DEFAULT_HEIGHT   =  768;

constexpr float2 g_uvs[NUM_TRIS][3] = 
{
    { {  1.0f, -1.0f }, { -1.0f, -1.0f }, { -1.0f,  1.0f } }, // Triangle ABC 
    { {  1.0f, -1.0f }, { -1.0f,  1.0f }, {  1.0f,  1.0f } }  // Triangle ACD
};


template <typename T>
struct SbtRecord
{
    __align__( OPTIX_SBT_RECORD_ALIGNMENT ) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

typedef SbtRecord<RayGenData>     RayGenSbtRecord;
typedef SbtRecord<MissData>       MissSbtRecord;
typedef SbtRecord<HitGroupData>   HitGroupSbtRecord;


void configureCamera( sutil::Camera& cam, const uint32_t width, const uint32_t height )
{
    cam.setEye( {0.0f, 0.0f, 1.5f} );
    cam.setLookat( {0.0f, 0.0f, 0.0f} );
    cam.setUp( {0.0f, 1.0f, 3.0f} );
    cam.setFovY( 45.0f );
    cam.setAspectRatio( (float)width / (float)height );
}


void printUsageAndExit( const char* argv0 )
{
    std::cerr << "Usage  : " << argv0 << " [options]\n";
    std::cerr << "Options: --file | -f <filename>      Specify file for image output\n";
    std::cerr << "         --help | -h                 Print this usage message\n";
    std::cerr << "         --dim=<width>x<height>      Set image dimensions; defaults to " 
              << DEFAULT_WIDTH << "x" << DEFAULT_HEIGHT << "\n";
    exit( 0 );
}


static void context_log_cb( unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: "
              << message << "\n";
}


int main( int argc, char* argv[] )
{
    std::string outfile;
    int         width  = DEFAULT_WIDTH;
    int         height = DEFAULT_HEIGHT;

    for( int i = 1; i < argc; ++i )
    {
        const std::string arg( argv[i] );
        if( arg == "--help" || arg == "-h" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "--file" || arg == "-f" )
        {
            if( i < argc - 1 )
            {
                outfile = argv[++i];
            }
            else
            {
                printUsageAndExit( argv[0] );
            }
        }
        else if( arg.substr( 0, 6 ) == "--dim=" )
        {
            const std::string dims_arg = arg.substr( 6 );
            sutil::parseDimensions( dims_arg.c_str(), width, height );
        }
        else
        {
            std::cerr << "Unknown option '" << arg << "'\n";
            printUsageAndExit( argv[0] );
        }
    }

    try
    {
        //
        // Initialize CUDA and create OptiX context
        //
        OptixDeviceContext context = nullptr;
        {
            // Initialize CUDA
            CUDA_CHECK( cudaFree( 0 ) );

            // Initialize the OptiX API, loading all API entry points
            OPTIX_CHECK( optixInit() );

            // Specify context options
            OptixDeviceContextOptions options = {};
            options.logCallbackFunction       = &context_log_cb;
            options.logCallbackLevel          = 4;

            // Associate a CUDA context (and therefore a specific GPU) with this
            // device context
            CUcontext cuCtx = 0;  // zero means take the current context
            OPTIX_CHECK( optixDeviceContextCreate( cuCtx, &options, &context ) );
        }

        //
        // create opacity micromap
        //
        CUdeviceptr d_omm_array = 0;
        {
            constexpr int NUM_MICRO_TRIS = 1 << ( OMM_SUBDIV_LEVEL*2 );
            constexpr int BITS_PER_STATE = 2;

            unsigned short omm_input_data[ NUM_TRIS ][ NUM_MICRO_TRIS / 16 * BITS_PER_STATE ] = {}; 

            // Calculate the texture coordinate at the micromesh vertices of the triangle and 
            // determine if the triangle is inside, outside, or spanning the boundary of the circle.
            // Note that the tex coords are in [-1, 1] and the circle is centered at uv=(0,0).
            auto evaluteOpacity = []( 
                const float2& bary0, 
                const float2& bary1, 
                const float2& bary2, 
                const float2* uvs 
                )
            {
                const float2 uv0 = computeUV(bary0, uvs[0], uvs[1], uvs[2] );
                const float2 uv1 = computeUV(bary1, uvs[0], uvs[1], uvs[2] );
                const float2 uv2 = computeUV(bary2, uvs[0], uvs[1], uvs[2] );
                const bool in_circle0 = inCircle( uv0 );
                const bool in_circle1 = inCircle( uv1 );
                const bool in_circle2 = inCircle( uv2 );
                if( in_circle0 && in_circle1 && in_circle2 ) 
                    // All 3 verts inside circle, mark transparent
                    return OPTIX_OPACITY_MICROMAP_STATE_TRANSPARENT;
                else if( !in_circle0 && !in_circle1 && !in_circle2 )
                    // All 3 verts outside circle, mark it as opaque
                    return OPTIX_OPACITY_MICROMAP_STATE_OPAQUE;
                else
                    // Mixture of verts inside and outside, mark as unknown and let AH evaluate
                    return OPTIX_OPACITY_MICROMAP_STATE_UNKNOWN_OPAQUE;
            };

            for( uint32_t uTriI = 0; uTriI < NUM_MICRO_TRIS; ++uTriI )
            {

                // Opacity micromaps for a circular cutout: Check if the micro
                // triangle area overlaps the circle, if so, it needs to be
                // marked as 'unknown'.  If all micro triangle vertices are
                // within the circle, it is marked as transparent.  Otherwise
                // it must be fully outside the circle and can be marked
                // opaque.  Note that this is not fully accurate as an edge may
                // still intersect the circle, we choose to ignore this detail
                // in this sample.
                //
                // NB: This computation must align with the anyhit program (We 
                //     are essentially baking the anyhit program at micro-tri 
                //     vertices).
                float2 bary0, bary1, bary2;
                optixMicromapIndexToBaseBarycentrics( uTriI, OMM_SUBDIV_LEVEL, bary0, bary1, bary2 );

                // first triangle (a,b,c)
                {
                    const int opacity = evaluteOpacity( bary0, bary1, bary2, g_uvs[0] );
                    omm_input_data[0][uTriI/8] |= opacity << ( uTriI%8 * 2 );
                }
                
                // second triangle (a,c,d)
                {
                    const int opacity = evaluteOpacity( bary0, bary1, bary2, g_uvs[1] );
                    omm_input_data[1][uTriI/8] |= opacity << ( uTriI%8 * 2 );
                }
            }
            
            // Copy the omm array to device
            CUdeviceptr  d_omm_input_data = 0;
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_omm_input_data ), sizeof( omm_input_data ) ) ); 
            CUDA_CHECK( cudaMemcpy( 
                reinterpret_cast<void*>( d_omm_input_data ),
                omm_input_data,
                sizeof( omm_input_data ), 
                cudaMemcpyHostToDevice 
            ) );

            //
            // Build micromap 
            //
            OptixOpacityMicromapHistogramEntry histogram{};
            histogram.count            = NUM_TRIS;
            histogram.format           = OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE;
            histogram.subdivisionLevel = OMM_SUBDIV_LEVEL;

            OptixOpacityMicromapArrayBuildInput build_input = {};
            build_input.flags                       = OPTIX_OPACITY_MICROMAP_FLAG_NONE;
            build_input.inputBuffer                 = d_omm_input_data;           
            build_input.numMicromapHistogramEntries = 1; 
            build_input.micromapHistogramEntries    = &histogram;

            OptixMicromapBufferSizes buffer_sizes = {};
            OPTIX_CHECK( optixOpacityMicromapArrayComputeMemoryUsage( context, &build_input, &buffer_sizes) );

            // Two OMMs, both with the same layout
            std::vector<OptixOpacityMicromapDesc> omm_descs =
            {                                                                                           
                {   
                    0,   // byteOffset for triangle 0                                       
                    OMM_SUBDIV_LEVEL, 
                    OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE 
                },
                {    
                    // byteOffset for triangle 1                                       
                    static_cast<unsigned int>( sizeof(omm_input_data[0]) ), 
                    OMM_SUBDIV_LEVEL,
                    OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE
                }
            };

            CUdeviceptr  d_omm_desc = 0;
            const size_t omm_desc_size_bytes = omm_descs.size() * sizeof(OptixOpacityMicromapDesc);
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_omm_desc ), omm_desc_size_bytes ) );
            CUDA_CHECK( cudaMemcpy( 
                reinterpret_cast<void*>( d_omm_desc ), 
                omm_descs.data(), 
                omm_desc_size_bytes, 
                cudaMemcpyHostToDevice 
            ) );

            build_input.perMicromapDescBuffer = d_omm_desc;
            build_input.perMicromapDescStrideInBytes = 0;

            CUdeviceptr d_temp_buffer = 0;
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_temp_buffer ), buffer_sizes.tempSizeInBytes ) );
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_omm_array ), buffer_sizes.outputSizeInBytes ) );

            OptixMicromapBuffers micromap_buffers = {};
            micromap_buffers.output               = d_omm_array;
            micromap_buffers.outputSizeInBytes    = buffer_sizes.outputSizeInBytes;
            micromap_buffers.temp                 = d_temp_buffer;
            micromap_buffers.tempSizeInBytes      = buffer_sizes.tempSizeInBytes;

            OPTIX_CHECK( optixOpacityMicromapArrayBuild( context, 0, &build_input, &micromap_buffers) );

            cudaFree( reinterpret_cast<void*>( d_omm_input_data ) );
            cudaFree( reinterpret_cast<void*>( d_omm_desc ) );
            cudaFree( reinterpret_cast<void*>( d_temp_buffer ) );
        }

        //
        // accel handling
        //
        OptixTraversableHandle gas_handle;
        CUdeviceptr            d_gas_output_buffer;
        {

            //
            // Create OMM input
            //
            OptixOpacityMicromapUsageCount usage_count={};
            usage_count.count  = NUM_TRIS; 
            // simple 2 state as the OMM perfectly matches the checkerboard pattern. 
            // 'unknown' states that are resolved in the anyhit program are not needed.
            usage_count.format = OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE;  
            usage_count.subdivisionLevel = OMM_SUBDIV_LEVEL;

            std::array<unsigned short, NUM_TRIS> omm_indices = { 0u, 1u };
            const size_t omm_indices_size_bytes = omm_indices.size() * sizeof( unsigned short ); 

            CUdeviceptr  d_omm_indices = 0;
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_omm_indices ), omm_indices_size_bytes ) );
            CUDA_CHECK( cudaMemcpy( 
                reinterpret_cast<void*>( d_omm_indices ),
                omm_indices.data(),
                omm_indices_size_bytes,
                cudaMemcpyHostToDevice
            ) );

            OptixBuildInputOpacityMicromap omm_input = {};
            omm_input.indexingMode           = OPTIX_OPACITY_MICROMAP_ARRAY_INDEXING_MODE_INDEXED;
            omm_input.opacityMicromapArray   = d_omm_array;
            omm_input.indexBuffer            = d_omm_indices;
            omm_input.indexSizeInBytes       = 2;
            omm_input.numMicromapUsageCounts = 1;
            omm_input.micromapUsageCounts    = &usage_count;

            //
            // Build GAS
            //

            // Use default options for simplicity.  In a real use case we would want to
            // enable compaction, etc
            OptixAccelBuildOptions accel_options = {};
            accel_options.buildFlags = OPTIX_BUILD_FLAG_NONE;
            accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;

            // Quad ABCD build input: simple list of two triangles:
            // * Triangle ABC
            // * Triangle ACD
            const std::array<float3, 6> vertices =
            { {
                  { -0.5f, -0.5f, 0.0f },
                  {  0.5f, -0.5f, 0.0f },
                  {  0.5f,  0.5f, 0.0f },

                  { -0.5f, -0.5f, 0.0f },
                  {  0.5f,  0.5f, 0.0f },
                  { -0.5f,  0.5f, 0.0f }
            } };

            const size_t vertices_size = sizeof( float3 )*vertices.size();
            CUdeviceptr d_vertices=0;
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_vertices ), vertices_size ) );
            CUDA_CHECK( cudaMemcpy(
                        reinterpret_cast<void*>( d_vertices ),
                        vertices.data(),
                        vertices_size,
                        cudaMemcpyHostToDevice
                        ) );

            // Our build input is a simple list of non-indexed triangle vertices
            const uint32_t triangle_input_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };
            OptixBuildInput triangle_input = {};
            triangle_input.type                          = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
            triangle_input.triangleArray.vertexFormat    = OPTIX_VERTEX_FORMAT_FLOAT3;
            triangle_input.triangleArray.numVertices     = static_cast<uint32_t>( vertices.size() );
            triangle_input.triangleArray.vertexBuffers   = &d_vertices;
            triangle_input.triangleArray.flags           = triangle_input_flags;
            triangle_input.triangleArray.numSbtRecords   = 1;
            triangle_input.triangleArray.opacityMicromap = omm_input;


            OptixAccelBufferSizes gas_buffer_sizes;
            OPTIX_CHECK( optixAccelComputeMemoryUsage(
                        context,
                        &accel_options,
                        &triangle_input,
                        1, // Number of build inputs
                        &gas_buffer_sizes
                        ) );
            CUdeviceptr d_temp_buffer_gas;
            CUDA_CHECK( cudaMalloc(
                        reinterpret_cast<void**>( &d_temp_buffer_gas ),
                        gas_buffer_sizes.tempSizeInBytes
                        ) );
            CUDA_CHECK( cudaMalloc(
                        reinterpret_cast<void**>( &d_gas_output_buffer ),
                        gas_buffer_sizes.outputSizeInBytes
                        ) );

            OPTIX_CHECK( optixAccelBuild(
                        context,
                        0,                  // CUDA stream
                        &accel_options,
                        &triangle_input,
                        1,                  // num build inputs
                        d_temp_buffer_gas,
                        gas_buffer_sizes.tempSizeInBytes,
                        d_gas_output_buffer,
                        gas_buffer_sizes.outputSizeInBytes,
                        &gas_handle,
                        nullptr,            // emitted property list
                        0                   // num emitted properties
                        ) );

            // We can now free the scratch space buffer used during build and the vertex
            // inputs, since they are not needed by our trivial shading method
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_temp_buffer_gas ) ) );
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_vertices        ) ) );
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_omm_indices ) ) );

        }

        //
        // Create module
        //
        OptixModule module = nullptr;
        OptixPipelineCompileOptions pipeline_compile_options = {};
        {
            OptixModuleCompileOptions module_compile_options = {};
#if !defined( NDEBUG )
            module_compile_options.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
            module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#endif

            pipeline_compile_options.usesMotionBlur        = false;
            pipeline_compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
            pipeline_compile_options.numPayloadValues      = 4;
            pipeline_compile_options.numAttributeValues    = 2;
#if !defined( NDEBUG ) 
            // Enables debug exceptions during optix launches.  This may incur
            // significant performance cost and should only be done during
            // development.
            pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_DEBUG         | 
                                                      OPTIX_EXCEPTION_FLAG_TRACE_DEPTH   | 
                                                      OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW;
#else
            pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
#endif
            pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
            pipeline_compile_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;
            pipeline_compile_options.allowOpacityMicromaps = true;

            size_t      inputSize  = 0;
            const char* input      = sutil::getInputData( OPTIX_SAMPLE_NAME, OPTIX_SAMPLE_DIR, "optixOpacityMicromap.cu", inputSize );

            OPTIX_CHECK_LOG( optixModuleCreateFromPTX(
                        context,
                        &module_compile_options,
                        &pipeline_compile_options,
                        input,
                        inputSize,
                        LOG, &LOG_SIZE,
                        &module
                        ) );
        }

        //
        // Create program groups
        //
        OptixProgramGroup raygen_prog_group   = nullptr;
        OptixProgramGroup miss_prog_group     = nullptr;
        OptixProgramGroup hitgroup_prog_group = nullptr;
        {
            OptixProgramGroupOptions program_group_options   = {}; // Initialize to zeros

            OptixProgramGroupDesc raygen_prog_group_desc    = {}; 
            raygen_prog_group_desc.kind                     = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
            raygen_prog_group_desc.raygen.module            = module;
            raygen_prog_group_desc.raygen.entryFunctionName = "__raygen__rg";
            OPTIX_CHECK_LOG( optixProgramGroupCreate(
                        context,
                        &raygen_prog_group_desc,
                        1,   // num program groups
                        &program_group_options,
                        LOG, &LOG_SIZE,
                        &raygen_prog_group
                        ) );

            OptixProgramGroupDesc miss_prog_group_desc  = {};
            miss_prog_group_desc.kind                   = OPTIX_PROGRAM_GROUP_KIND_MISS;
            miss_prog_group_desc.miss.module            = module;
            miss_prog_group_desc.miss.entryFunctionName = "__miss__ms";
            OPTIX_CHECK_LOG( optixProgramGroupCreate(
                        context,
                        &miss_prog_group_desc,
                        1,   // num program groups
                        &program_group_options,
                        LOG, &LOG_SIZE,
                        &miss_prog_group
                        ) );

            OptixProgramGroupDesc hitgroup_prog_group_desc = {};
            hitgroup_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
            hitgroup_prog_group_desc.hitgroup.moduleCH            = module;
            hitgroup_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__ch";
            hitgroup_prog_group_desc.hitgroup.moduleAH            = module;
            hitgroup_prog_group_desc.hitgroup.entryFunctionNameAH = "__anyhit__opacity";
            OPTIX_CHECK_LOG( optixProgramGroupCreate(
                        context,
                        &hitgroup_prog_group_desc,
                        1,   // num program groups
                        &program_group_options,
                        LOG, &LOG_SIZE,
                        &hitgroup_prog_group
                        ) );
        }

        //
        // Link pipeline
        //
        OptixPipeline pipeline = nullptr;
        {
            const uint32_t    max_trace_depth  = 1;
            OptixProgramGroup program_groups[] = { raygen_prog_group, miss_prog_group, hitgroup_prog_group };

            OptixPipelineLinkOptions pipeline_link_options = {};
            pipeline_link_options.maxTraceDepth            = max_trace_depth;
            OPTIX_CHECK_LOG( optixPipelineCreate(
                        context,
                        &pipeline_compile_options,
                        &pipeline_link_options,
                        program_groups,
                        sizeof( program_groups ) / sizeof( program_groups[0] ),
                        LOG, &LOG_SIZE,
                        &pipeline
                        ) );

            OptixStackSizes stack_sizes = {};
            for( auto& prog_group : program_groups )
            {
                OPTIX_CHECK( optixUtilAccumulateStackSizes( prog_group, &stack_sizes ) );
            }

            uint32_t direct_callable_stack_size_from_traversal;
            uint32_t direct_callable_stack_size_from_state;
            uint32_t continuation_stack_size;
            OPTIX_CHECK( optixUtilComputeStackSizes( &stack_sizes, max_trace_depth,
                                                     0,  // maxCCDepth
                                                     0,  // maxDCDEpth
                                                     &direct_callable_stack_size_from_traversal,
                                                     &direct_callable_stack_size_from_state, &continuation_stack_size ) );
            OPTIX_CHECK( optixPipelineSetStackSize( pipeline, direct_callable_stack_size_from_traversal,
                                                    direct_callable_stack_size_from_state, continuation_stack_size,
                                                    1  // maxTraversableDepth
                                                    ) );
        }

        //
        // Set up shader binding table
        //
        OptixShaderBindingTable sbt = {};
        CUdeviceptr             d_uvs=0;
        {
            CUdeviceptr  raygen_record;
            const size_t raygen_record_size = sizeof( RayGenSbtRecord );
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &raygen_record ), raygen_record_size ) );
            RayGenSbtRecord rg_sbt;
            OPTIX_CHECK( optixSbtRecordPackHeader( raygen_prog_group, &rg_sbt ) );
            CUDA_CHECK( cudaMemcpy(
                        reinterpret_cast<void*>( raygen_record ),
                        &rg_sbt,
                        raygen_record_size,
                        cudaMemcpyHostToDevice
                        ) );

            CUdeviceptr miss_record;
            size_t      miss_record_size = sizeof( MissSbtRecord );
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &miss_record ), miss_record_size ) );
            MissSbtRecord ms_sbt;
            ms_sbt.data = { 0.01f, 0.01f, 0.01f };
            OPTIX_CHECK( optixSbtRecordPackHeader( miss_prog_group, &ms_sbt ) );
            CUDA_CHECK( cudaMemcpy(
                        reinterpret_cast<void*>( miss_record ),
                        &ms_sbt,
                        miss_record_size,
                        cudaMemcpyHostToDevice
                        ) );

            //const size_t uvs_size_bytes = g_uvs.size() * sizeof( float2 );
            const size_t uvs_size_bytes = sizeof( g_uvs );
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_uvs ), uvs_size_bytes ) );
            CUDA_CHECK( cudaMemcpy( 
                        reinterpret_cast<void*>( d_uvs ), 
                        g_uvs,
                        uvs_size_bytes,
                        cudaMemcpyHostToDevice
            ) );

            CUdeviceptr hitgroup_record;
            size_t      hitgroup_record_size = sizeof( HitGroupSbtRecord );
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &hitgroup_record ), hitgroup_record_size ) );
            HitGroupSbtRecord hg_sbt;
            hg_sbt.data.uvs = reinterpret_cast<float2*>( d_uvs );
            OPTIX_CHECK( optixSbtRecordPackHeader( hitgroup_prog_group, &hg_sbt ) );
            CUDA_CHECK( cudaMemcpy(
                        reinterpret_cast<void*>( hitgroup_record ),
                        &hg_sbt,
                        hitgroup_record_size,
                        cudaMemcpyHostToDevice
                        ) );

            sbt.raygenRecord                = raygen_record;
            sbt.missRecordBase              = miss_record;
            sbt.missRecordStrideInBytes     = sizeof( MissSbtRecord );
            sbt.missRecordCount             = 1;
            sbt.hitgroupRecordBase          = hitgroup_record;
            sbt.hitgroupRecordStrideInBytes = sizeof( HitGroupSbtRecord );
            sbt.hitgroupRecordCount         = 1;
        }

        sutil::CUDAOutputBuffer<uchar4> output_buffer( sutil::CUDAOutputBufferType::CUDA_DEVICE, width, height );

        //
        // launch
        //
        {
            CUstream stream;
            CUDA_CHECK( cudaStreamCreate( &stream ) );

            sutil::Camera cam;
            configureCamera( cam, width, height );

            Params params;
            params.image        = output_buffer.map();
            params.image_width  = width;
            params.image_height = height;
            params.handle       = gas_handle;
            params.cam_eye      = cam.eye();
            cam.UVWFrame( params.cam_u, params.cam_v, params.cam_w );

            CUdeviceptr d_param;
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_param ), sizeof( Params ) ) );
            CUDA_CHECK( cudaMemcpy(
                        reinterpret_cast<void*>( d_param ),
                        &params, sizeof( params ),
                        cudaMemcpyHostToDevice
                        ) );

            OPTIX_CHECK( optixLaunch( pipeline, stream, d_param, sizeof( Params ), &sbt, width, height, /*depth=*/1 ) );
            CUDA_SYNC_CHECK();

            output_buffer.unmap();
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_param ) ) );
        }

        //
        // Display results
        //
        {
            sutil::ImageBuffer buffer;
            buffer.data         = output_buffer.getHostPointer();
            buffer.width        = width;
            buffer.height       = height;
            buffer.pixel_format = sutil::BufferImageFormat::UNSIGNED_BYTE4;
            if( outfile.empty() )
                sutil::displayBufferWindow( argv[0], buffer );
            else
                sutil::saveImage( outfile.c_str(), buffer, false );
        }

        //
        // Cleanup
        //
        {
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( sbt.raygenRecord       ) ) );
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( sbt.missRecordBase     ) ) );
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( sbt.hitgroupRecordBase ) ) );
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_gas_output_buffer    ) ) );
            CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_uvs                  ) ) );

            OPTIX_CHECK( optixPipelineDestroy( pipeline ) );
            OPTIX_CHECK( optixProgramGroupDestroy( hitgroup_prog_group ) );
            OPTIX_CHECK( optixProgramGroupDestroy( miss_prog_group ) );
            OPTIX_CHECK( optixProgramGroupDestroy( raygen_prog_group ) );
            OPTIX_CHECK( optixModuleDestroy( module ) );

            OPTIX_CHECK( optixDeviceContextDestroy( context ) );
        }
    }
    catch( std::exception& e )
    {
        std::cerr << "Caught exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
