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

#include <cuda_runtime.h>

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stubs.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <optix_stack_size.h>

#include <cuda/whitted.h>

#include <sutil/Exception.h>
#include <sutil/Record.h>
#ifndef OPTIX_CONSOLE_PROJECT
#include <sutil/sutil.h>
#endif

#include <fstream>
#include <iomanip>
#include <vector>


// Globals
const int max_trace = 12;

// Local types
typedef sutil::Record<whitted::HitGroupData> HitGroupRecord;

const uint32_t OBJ_COUNT = 3;

struct OptixConsoleState
{
    OptixDeviceContext     context             = 0;
    OptixTraversableHandle gas_handle          = {};
    CUdeviceptr            d_gas_output_buffer = {};

    OptixModule geometry_module = 0;
    OptixModule camera_module   = 0;
    OptixModule shading_module  = 0;
    OptixModule sphere_module   = 0;

    OptixProgramGroup raygen_prog_group                 = 0;
    OptixProgramGroup radiance_miss_prog_group          = 0;
    OptixProgramGroup occlusion_miss_prog_group         = 0;
    OptixProgramGroup radiance_glass_sphere_prog_group  = 0;
    OptixProgramGroup occlusion_glass_sphere_prog_group = 0;
    OptixProgramGroup radiance_metal_sphere_prog_group  = 0;
    OptixProgramGroup occlusion_metal_sphere_prog_group = 0;
    OptixProgramGroup radiance_floor_prog_group         = 0;
    OptixProgramGroup occlusion_floor_prog_group        = 0;

    OptixPipeline               pipeline                 = 0;
    OptixPipelineCompileOptions pipeline_compile_options = {};

    CUstream               stream = 0;
    whitted::LaunchParams  params;
    whitted::LaunchParams* d_params = nullptr;

    OptixShaderBindingTable sbt = {};
};

// Metal sphere, glass sphere, floor
const GeometryData::Sphere g_sphere = {
    { 2.0f, 1.5f, -2.5f },  // center
    1.0f                    // radius
};
const GeometryData::SphereShell g_sphere_shell = {
    { 4.0f, 2.3f, -4.0f },  // center
    0.96f,                  // radius1
    1.0f                    // radius2
};
const GeometryData::Parallelogram g_floor( make_float3( 32.0f, 0.0f, 0.0f ),    // v1
                                           make_float3( 0.0f, 0.0f, 16.0f ),    // v2
                                           make_float3( -16.0f, 0.01f, -8.0f )  // anchor
);

//  Helper functions
void printUsageAndExit( const char* argv0 )
{
    std::cerr << "Usage  : " << argv0 << " [options]\n";
    std::cerr << "Options: --file | -f <filename>      File for image output\n";
    std::cerr << "         --help | -h                 Print this usage message\n";
    exit( 0 );
}

void initLaunchParams( OptixConsoleState& state )
{
    state.params.width  = 48u * 2u;
    state.params.height = 32u * 2u;

    state.params.eye = make_float3( 8.0f, 2.0f, -4.0f );
    state.params.U   = make_float3( 0.0f, 0.0f, -2.315887f );
    state.params.V   = make_float3( 0.173205f, 2.309401f, 0.0f );
    state.params.W   = make_float3( -4.0f, 0.3f, 0.0f );

    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.params.accum_buffer ),
                            state.params.width * state.params.height * sizeof( float4 ) ) );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.params.frame_buffer ),
                            state.params.width * state.params.height * sizeof( uchar4 ) ) );

    state.params.subframe_index = 0u;

    // Set ambient light color and point light position
    std::vector<Light> lights( 2 );
    lights[0].type            = Light::Type::AMBIENT;
    lights[0].ambient.color   = make_float3( 0.4f, 0.4f, 0.4f );
    lights[1].type            = Light::Type::POINT;
    lights[1].point.color     = make_float3( 1.0f, 1.0f, 1.0f );
    lights[1].point.intensity = 1.0f;
    lights[1].point.position  = make_float3( 60.0f, 40.0f, 0.0f );
    lights[1].point.falloff   = Light::Falloff::QUADRATIC;

    state.params.lights.count = static_cast<unsigned int>( lights.size() );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.params.lights.data ), lights.size() * sizeof( Light ) ) );
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( state.params.lights.data ), lights.data(),
                            lights.size() * sizeof( Light ), cudaMemcpyHostToDevice ) );
    state.params.miss_color = { 0.34f, 0.55f, 0.85f };

    state.params.max_depth     = max_trace;
    state.params.scene_epsilon = 1.e-4f;

    CUDA_CHECK( cudaStreamCreate( &state.stream ) );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_params ), sizeof( whitted::LaunchParams ) ) );

    state.params.handle = state.gas_handle;
}

inline OptixAabb sphere_bound( float3 center, float radius )
{
    float3 m_min = center - radius;
    float3 m_max = center + radius;

    return { m_min.x, m_min.y, m_min.z, m_max.x, m_max.y, m_max.z };
}

inline OptixAabb parallelogram_bound( float3 v1, float3 v2, float3 anchor )
{
    // v1 and v2 are scaled by 1./length^2.  Rescale back to normal for the bounds computation.
    const float3 tv1 = v1 / dot( v1, v1 );
    const float3 tv2 = v2 / dot( v2, v2 );
    const float3 p00 = anchor;
    const float3 p01 = anchor + tv1;
    const float3 p10 = anchor + tv2;
    const float3 p11 = anchor + tv1 + tv2;

    float3 m_min = fminf( fminf( p00, p01 ), fminf( p10, p11 ) );
    float3 m_max = fmaxf( fmaxf( p00, p01 ), fmaxf( p10, p11 ) );
    return { m_min.x, m_min.y, m_min.z, m_max.x, m_max.y, m_max.z };
}

static void buildGas( const OptixConsoleState&      state,
                      const OptixAccelBuildOptions& accel_options,
                      const OptixBuildInput&        build_input,
                      OptixTraversableHandle&       gas_handle,
                      CUdeviceptr&                  d_gas_output_buffer )
{
    OptixAccelBufferSizes gas_buffer_sizes;
    CUdeviceptr           d_temp_buffer_gas;

    OPTIX_CHECK( optixAccelComputeMemoryUsage( state.context, &accel_options, &build_input, 1, &gas_buffer_sizes ) );

    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_temp_buffer_gas ), gas_buffer_sizes.tempSizeInBytes ) );

    // non-compacted output and size of compacted GAS
    CUdeviceptr d_buffer_temp_output_gas_and_compacted_size;
    size_t      compactedSizeOffset = ( ( gas_buffer_sizes.outputSizeInBytes + 8ull - 1 ) / 8ull ) * 8ull;
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_buffer_temp_output_gas_and_compacted_size ), compactedSizeOffset + 8 ) );

    OptixAccelEmitDesc emitProperty = {};
    emitProperty.type               = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
    emitProperty.result = (CUdeviceptr)( (char*)d_buffer_temp_output_gas_and_compacted_size + compactedSizeOffset );

    OPTIX_CHECK( optixAccelBuild( state.context, 0, &accel_options, &build_input, 1, d_temp_buffer_gas,
                                  gas_buffer_sizes.tempSizeInBytes, d_buffer_temp_output_gas_and_compacted_size,
                                  gas_buffer_sizes.outputSizeInBytes, &gas_handle, &emitProperty, 1 ) );

    CUDA_CHECK( cudaFree( (void*)d_temp_buffer_gas ) );

    size_t compacted_gas_size;
    CUDA_CHECK( cudaMemcpy( &compacted_gas_size, (void*)emitProperty.result, sizeof( size_t ), cudaMemcpyDeviceToHost ) );

    if( compacted_gas_size < gas_buffer_sizes.outputSizeInBytes )
    {
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_gas_output_buffer ), compacted_gas_size ) );

        // use handle as input and output
        OPTIX_CHECK( optixAccelCompact( state.context, 0, gas_handle, d_gas_output_buffer, compacted_gas_size, &gas_handle ) );

        CUDA_CHECK( cudaFree( (void*)d_buffer_temp_output_gas_and_compacted_size ) );
    }
    else
    {
        d_gas_output_buffer = d_buffer_temp_output_gas_and_compacted_size;
    }
}

void createGeometry( OptixConsoleState& state )
{
    // Load AABB into device memory
    OptixAabb   aabb[OBJ_COUNT] = { sphere_bound( g_sphere.center, g_sphere.radius ),
                                  sphere_bound( g_sphere_shell.center, g_sphere_shell.radius2 ),
                                  parallelogram_bound( g_floor.v1, g_floor.v2, g_floor.anchor ) };
    CUdeviceptr d_aabb;

    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_aabb ), OBJ_COUNT * sizeof( OptixAabb ) ) );
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_aabb ), &aabb, OBJ_COUNT * sizeof( OptixAabb ), cudaMemcpyHostToDevice ) );

    // Setup AABB build input
    uint32_t aabb_input_flags[] = {
        /* flags for metal sphere */
        OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT,
        /* flag for glass sphere */
        OPTIX_GEOMETRY_FLAG_REQUIRE_SINGLE_ANYHIT_CALL,
        /* flag for floor */
        OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT,
    };
    /* TODO: This API cannot control flags for different ray type */

    const uint32_t sbt_index[] = { 0, 1, 2 };
    CUdeviceptr    d_sbt_index;

    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_sbt_index ), sizeof( sbt_index ) ) );
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_sbt_index ), sbt_index, sizeof( sbt_index ), cudaMemcpyHostToDevice ) );

    OptixBuildInput aabb_input                                = {};
    aabb_input.type                                           = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
    aabb_input.customPrimitiveArray.aabbBuffers               = &d_aabb;
    aabb_input.customPrimitiveArray.flags                     = aabb_input_flags;
    aabb_input.customPrimitiveArray.numSbtRecords             = OBJ_COUNT;
    aabb_input.customPrimitiveArray.numPrimitives             = OBJ_COUNT;
    aabb_input.customPrimitiveArray.sbtIndexOffsetBuffer      = d_sbt_index;
    aabb_input.customPrimitiveArray.sbtIndexOffsetSizeInBytes = sizeof( uint32_t );
    aabb_input.customPrimitiveArray.primitiveIndexOffset      = 0;

    OptixAccelBuildOptions accel_options = {
        OPTIX_BUILD_FLAG_ALLOW_COMPACTION,  // buildFlags
        OPTIX_BUILD_OPERATION_BUILD         // operation
    };

    buildGas( state, accel_options, aabb_input, state.gas_handle, state.d_gas_output_buffer );

    CUDA_CHECK( cudaFree( (void*)d_aabb ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_sbt_index ) ) );
}

std::string readFile( const std::string& filename )
{
    std::string   contents;
    std::ifstream file( filename, std::ios::binary );
    if( file.good() )
    {
        std::vector<unsigned char> buffer = std::vector<unsigned char>( std::istreambuf_iterator<char>( file ), {} );
        contents.assign( buffer.begin(), buffer.end() );
    }
    else
    {
        std::cerr << "Error opening " << filename << ": " << strerror( errno ) << "\n";
    }
    return contents;
}

void createModules( OptixConsoleState& state )
{
    OptixModuleCompileOptions module_compile_options = {};
#if !defined( NDEBUG )
    module_compile_options.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
    module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#endif

#ifdef OPTIX_CONSOLE_PROJECT
    std::string geometry_ir         = readFile( CUDA_INT_DIR "/geometry.cu.optixir" );
    const char* geometry_input      = geometry_ir.c_str();
    size_t      geometry_input_size = geometry_ir.size();

    std::string camera_ir         = readFile( CUDA_INT_DIR "/camera.cu.optixir" );
    const char* camera_input      = camera_ir.c_str();
    size_t      camera_input_size = camera_ir.size();

    std::string shading_ir         = readFile( CUDA_INT_DIR "/shading.cu.optixir" );
    const char* shading_input      = shading_ir.c_str();
    size_t      shading_input_size = shading_ir.size();

    std::string sphere_ir         = readFile( CUDA_INT_DIR "/sphere.cu.optixir" );
    const char* sphere_input      = sphere_ir.c_str();
    size_t      sphere_input_size = sphere_ir.size();
#else
    size_t      geometry_input_size = 0;
    const char* geometry_input      = sutil::getInputData( nullptr, nullptr, "geometry.cu", geometry_input_size );

    size_t      camera_input_size = 0;
    const char* camera_input      = sutil::getInputData( nullptr, nullptr, "camera.cu", camera_input_size );

    size_t      shading_input_size = 0;
    const char* shading_input      = sutil::getInputData( nullptr, nullptr, "shading.cu", shading_input_size );

    size_t      sphere_input_size = 0;
    const char* sphere_input      = sutil::getInputData( nullptr, nullptr, "sphere.cu", sphere_input_size );
#endif

    OPTIX_CHECK_LOG( optixModuleCreateFromPTX( state.context, &module_compile_options, &state.pipeline_compile_options,
                                               geometry_input, geometry_input_size, LOG, &LOG_SIZE, &state.geometry_module ) );
    OPTIX_CHECK_LOG( optixModuleCreateFromPTX( state.context, &module_compile_options, &state.pipeline_compile_options,
                                               camera_input, camera_input_size, LOG, &LOG_SIZE, &state.camera_module ) );
    OPTIX_CHECK_LOG( optixModuleCreateFromPTX( state.context, &module_compile_options, &state.pipeline_compile_options,
                                               shading_input, shading_input_size, LOG, &LOG_SIZE, &state.shading_module ) );
    OPTIX_CHECK_LOG( optixModuleCreateFromPTX( state.context, &module_compile_options, &state.pipeline_compile_options,
                                               sphere_input, sphere_input_size, LOG, &LOG_SIZE, &state.sphere_module ) );
}

static void createCameraProgram( OptixConsoleState& state, std::vector<OptixProgramGroup>& program_groups )
{
    OptixProgramGroup        cam_prog_group;
    OptixProgramGroupOptions cam_prog_group_options = {};
    OptixProgramGroupDesc    cam_prog_group_desc    = {};
    cam_prog_group_desc.kind                        = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    cam_prog_group_desc.raygen.module               = state.camera_module;
    cam_prog_group_desc.raygen.entryFunctionName    = "__raygen__pinhole_camera";

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &cam_prog_group_desc, 1, &cam_prog_group_options, LOG,
                                              &LOG_SIZE, &cam_prog_group ) );

    program_groups.push_back( cam_prog_group );
    state.raygen_prog_group = cam_prog_group;
}

static void createGlassSphereProgram( OptixConsoleState& state, std::vector<OptixProgramGroup>& program_groups )
{
    OptixProgramGroup        radiance_sphere_prog_group;
    OptixProgramGroupOptions radiance_sphere_prog_group_options  = {};
    OptixProgramGroupDesc    radiance_sphere_prog_group_desc     = {};
    radiance_sphere_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    radiance_sphere_prog_group_desc.hitgroup.moduleIS            = state.geometry_module;
    radiance_sphere_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__sphere_shell";
    radiance_sphere_prog_group_desc.hitgroup.moduleCH            = state.shading_module;
    radiance_sphere_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__glass_radiance";
    radiance_sphere_prog_group_desc.hitgroup.moduleAH            = nullptr;
    radiance_sphere_prog_group_desc.hitgroup.entryFunctionNameAH = nullptr;

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &radiance_sphere_prog_group_desc, 1, &radiance_sphere_prog_group_options,
                                              LOG, &LOG_SIZE, &radiance_sphere_prog_group ) );

    program_groups.push_back( radiance_sphere_prog_group );
    state.radiance_glass_sphere_prog_group = radiance_sphere_prog_group;

    OptixProgramGroup        occlusion_sphere_prog_group;
    OptixProgramGroupOptions occlusion_sphere_prog_group_options  = {};
    OptixProgramGroupDesc    occlusion_sphere_prog_group_desc     = {};
    occlusion_sphere_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    occlusion_sphere_prog_group_desc.hitgroup.moduleIS            = state.geometry_module;
    occlusion_sphere_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__sphere_shell";
    occlusion_sphere_prog_group_desc.hitgroup.moduleCH            = nullptr;
    occlusion_sphere_prog_group_desc.hitgroup.entryFunctionNameCH = nullptr;
    occlusion_sphere_prog_group_desc.hitgroup.moduleAH            = state.shading_module;
    occlusion_sphere_prog_group_desc.hitgroup.entryFunctionNameAH = "__anyhit__glass_occlusion";

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &occlusion_sphere_prog_group_desc, 1, &occlusion_sphere_prog_group_options,
                                              LOG, &LOG_SIZE, &occlusion_sphere_prog_group ) );

    program_groups.push_back( occlusion_sphere_prog_group );
    state.occlusion_glass_sphere_prog_group = occlusion_sphere_prog_group;
}

static void createMetalSphereProgram( OptixConsoleState& state, std::vector<OptixProgramGroup>& program_groups )
{
    OptixProgramGroup        radiance_sphere_prog_group;
    OptixProgramGroupOptions radiance_sphere_prog_group_options  = {};
    OptixProgramGroupDesc    radiance_sphere_prog_group_desc     = {};
    radiance_sphere_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP,
    radiance_sphere_prog_group_desc.hitgroup.moduleIS            = state.sphere_module;
    radiance_sphere_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__sphere";
    radiance_sphere_prog_group_desc.hitgroup.moduleCH            = state.shading_module;
    radiance_sphere_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__metal_radiance";
    radiance_sphere_prog_group_desc.hitgroup.moduleAH            = nullptr;
    radiance_sphere_prog_group_desc.hitgroup.entryFunctionNameAH = nullptr;

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &radiance_sphere_prog_group_desc, 1, &radiance_sphere_prog_group_options,
                                              LOG, &LOG_SIZE, &radiance_sphere_prog_group ) );

    program_groups.push_back( radiance_sphere_prog_group );
    state.radiance_metal_sphere_prog_group = radiance_sphere_prog_group;

    OptixProgramGroup        occlusion_sphere_prog_group;
    OptixProgramGroupOptions occlusion_sphere_prog_group_options  = {};
    OptixProgramGroupDesc    occlusion_sphere_prog_group_desc     = {};
    occlusion_sphere_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP,
    occlusion_sphere_prog_group_desc.hitgroup.moduleIS            = state.sphere_module;
    occlusion_sphere_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__sphere";
    occlusion_sphere_prog_group_desc.hitgroup.moduleCH            = state.shading_module;
    occlusion_sphere_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__full_occlusion";
    occlusion_sphere_prog_group_desc.hitgroup.moduleAH            = nullptr;
    occlusion_sphere_prog_group_desc.hitgroup.entryFunctionNameAH = nullptr;

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &occlusion_sphere_prog_group_desc, 1, &occlusion_sphere_prog_group_options,
                                              LOG, &LOG_SIZE, &occlusion_sphere_prog_group ) );

    program_groups.push_back( occlusion_sphere_prog_group );
    state.occlusion_metal_sphere_prog_group = occlusion_sphere_prog_group;
}

static void createFloorProgram( OptixConsoleState& state, std::vector<OptixProgramGroup>& program_groups )
{
    OptixProgramGroup        radiance_floor_prog_group;
    OptixProgramGroupOptions radiance_floor_prog_group_options  = {};
    OptixProgramGroupDesc    radiance_floor_prog_group_desc     = {};
    radiance_floor_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    radiance_floor_prog_group_desc.hitgroup.moduleIS            = state.geometry_module;
    radiance_floor_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__parallelogram";
    radiance_floor_prog_group_desc.hitgroup.moduleCH            = state.shading_module;
    radiance_floor_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__checker_radiance";
    radiance_floor_prog_group_desc.hitgroup.moduleAH            = nullptr;
    radiance_floor_prog_group_desc.hitgroup.entryFunctionNameAH = nullptr;

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &radiance_floor_prog_group_desc, 1, &radiance_floor_prog_group_options,
                                              LOG, &LOG_SIZE, &radiance_floor_prog_group ) );

    program_groups.push_back( radiance_floor_prog_group );
    state.radiance_floor_prog_group = radiance_floor_prog_group;

    OptixProgramGroup        occlusion_floor_prog_group;
    OptixProgramGroupOptions occlusion_floor_prog_group_options  = {};
    OptixProgramGroupDesc    occlusion_floor_prog_group_desc     = {};
    occlusion_floor_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    occlusion_floor_prog_group_desc.hitgroup.moduleIS            = state.geometry_module;
    occlusion_floor_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__parallelogram";
    occlusion_floor_prog_group_desc.hitgroup.moduleCH            = state.shading_module;
    occlusion_floor_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__full_occlusion";
    occlusion_floor_prog_group_desc.hitgroup.moduleAH            = nullptr;
    occlusion_floor_prog_group_desc.hitgroup.entryFunctionNameAH = nullptr;

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &occlusion_floor_prog_group_desc, 1, &occlusion_floor_prog_group_options,
                                              LOG, &LOG_SIZE, &occlusion_floor_prog_group ) );

    program_groups.push_back( occlusion_floor_prog_group );
    state.occlusion_floor_prog_group = occlusion_floor_prog_group;
}

static void createMissProgram( OptixConsoleState& state, std::vector<OptixProgramGroup>& program_groups )
{
    OptixProgramGroupOptions miss_prog_group_options = {};
    OptixProgramGroupDesc    miss_prog_group_desc    = {};
    miss_prog_group_desc.kind                        = OPTIX_PROGRAM_GROUP_KIND_MISS;
    miss_prog_group_desc.miss.module                 = state.shading_module;
    miss_prog_group_desc.miss.entryFunctionName      = "__miss__constant_bg";

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &miss_prog_group_desc, 1, &miss_prog_group_options, LOG,
                                              &LOG_SIZE, &state.radiance_miss_prog_group ) );

    program_groups.push_back( state.radiance_miss_prog_group );

    miss_prog_group_desc.miss = {
        nullptr,  // module
        nullptr   // entryFunctionName
    };
    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &miss_prog_group_desc, 1, &miss_prog_group_options, LOG,
                                              &LOG_SIZE, &state.occlusion_miss_prog_group ) );

    program_groups.push_back( state.occlusion_miss_prog_group );
}

void createPipeline( OptixConsoleState& state )
{
    std::vector<OptixProgramGroup> program_groups;

    state.pipeline_compile_options = {
        false,                                          // usesMotionBlur
        OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS,  // traversableGraphFlags
        5,
        /* RadiancePRD uses 5 payloads */  // numPayloadValues
        5,
        /* Parallelogram intersection uses 5 attrs */  // numAttributeValues
        OPTIX_EXCEPTION_FLAG_NONE,                     // exceptionFlags
        "params"                                       // pipelineLaunchParamsVariableName
    };

    // Prepare program groups
    createModules( state );
    createCameraProgram( state, program_groups );
    createGlassSphereProgram( state, program_groups );
    createMetalSphereProgram( state, program_groups );
    createFloorProgram( state, program_groups );
    createMissProgram( state, program_groups );

    // Link program groups to pipeline
    OptixPipelineLinkOptions pipeline_link_options = {
        max_trace,                      // maxTraceDepth
        OPTIX_COMPILE_DEBUG_LEVEL_FULL  // debugLevel
    };
    OPTIX_CHECK_LOG( optixPipelineCreate( state.context, &state.pipeline_compile_options, &pipeline_link_options,
                                          program_groups.data(), static_cast<unsigned int>( program_groups.size() ),
                                          LOG, &LOG_SIZE, &state.pipeline ) );

    OptixStackSizes stack_sizes = {};
    for( auto& prog_group : program_groups )
    {
        OPTIX_CHECK( optixUtilAccumulateStackSizes( prog_group, &stack_sizes ) );
    }

    uint32_t direct_callable_stack_size_from_traversal;
    uint32_t direct_callable_stack_size_from_state;
    uint32_t continuation_stack_size;
    OPTIX_CHECK( optixUtilComputeStackSizes( &stack_sizes, max_trace,
                                             0,  // maxCCDepth
                                             0,  // maxDCDepth
                                             &direct_callable_stack_size_from_traversal,
                                             &direct_callable_stack_size_from_state, &continuation_stack_size ) );
    OPTIX_CHECK( optixPipelineSetStackSize( state.pipeline, direct_callable_stack_size_from_traversal,
                                            direct_callable_stack_size_from_state, continuation_stack_size,
                                            1  // maxTraversableDepth
                                            ) );
}

void createSBT( OptixConsoleState& state )
{
    // Raygen program record
    {
        CUdeviceptr d_raygen_record;
        size_t      sizeof_raygen_record = sizeof( sutil::EmptyRecord );
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_raygen_record ), sizeof_raygen_record ) );

        sutil::EmptyRecord rg_sbt;
        optixSbtRecordPackHeader( state.raygen_prog_group, &rg_sbt );

        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_raygen_record ), &rg_sbt, sizeof_raygen_record, cudaMemcpyHostToDevice ) );

        state.sbt.raygenRecord = d_raygen_record;
    }

    // Miss program record
    {
        CUdeviceptr d_miss_record;
        size_t      sizeof_miss_record = sizeof( sutil::EmptyRecord );
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_miss_record ), sizeof_miss_record * whitted::RAY_TYPE_COUNT ) );

        sutil::EmptyRecord ms_sbt[whitted::RAY_TYPE_COUNT];
        optixSbtRecordPackHeader( state.radiance_miss_prog_group, &ms_sbt[0] );
        optixSbtRecordPackHeader( state.occlusion_miss_prog_group, &ms_sbt[1] );

        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_miss_record ), ms_sbt,
                                sizeof_miss_record * whitted::RAY_TYPE_COUNT, cudaMemcpyHostToDevice ) );

        state.sbt.missRecordBase          = d_miss_record;
        state.sbt.missRecordCount         = whitted::RAY_TYPE_COUNT;
        state.sbt.missRecordStrideInBytes = static_cast<uint32_t>( sizeof_miss_record );
    }

    // Hitgroup program record
    {
        const size_t   count_records = whitted::RAY_TYPE_COUNT * OBJ_COUNT;
        HitGroupRecord hitgroup_records[count_records];

        // Note: Fill SBT record array the same order like AS is built.
        int sbt_idx = 0;

        // Metal Sphere
        OPTIX_CHECK( optixSbtRecordPackHeader( state.radiance_metal_sphere_prog_group, &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.geometry_data.setSphere( g_sphere );
        hitgroup_records[sbt_idx].data.material_data.metal  = {
            { 0.2f, 0.5f, 0.5f },  // Ka
            { 0.2f, 0.7f, 0.8f },  // Kd
            { 0.9f, 0.9f, 0.9f },  // Ks
            { 0.5f, 0.5f, 0.5f },  // Kr
            64,                    // phong_exp
        };
        sbt_idx++;

        OPTIX_CHECK( optixSbtRecordPackHeader( state.occlusion_metal_sphere_prog_group, &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.geometry_data.setSphere( g_sphere );
        sbt_idx++;

        // Glass Sphere
        OPTIX_CHECK( optixSbtRecordPackHeader( state.radiance_glass_sphere_prog_group, &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.geometry_data.setSphereShell( g_sphere_shell );
        hitgroup_records[sbt_idx].data.material_data.glass        = {
            1e-2f,                                         // importance_cutoff
            { 0.034f, 0.055f, 0.085f },                    // cutoff_color
            3.0f,                                          // fresnel_exponent
            0.1f,                                          // fresnel_minimum
            1.0f,                                          // fresnel_maximum
            1.4f,                                          // refraction_index
            { 1.0f, 1.0f, 1.0f },                          // refraction_color
            { 1.0f, 1.0f, 1.0f },                          // reflection_color
            { logf( .83f ), logf( .83f ), logf( .83f ) },  // extinction_constant
            { 0.6f, 0.6f, 0.6f },                          // shadow_attenuation
            10,                                            // refraction_maxdepth
            5                                              // reflection_maxdepth
        };
        sbt_idx++;

        OPTIX_CHECK( optixSbtRecordPackHeader( state.occlusion_glass_sphere_prog_group, &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.geometry_data.setSphereShell( g_sphere_shell );
        hitgroup_records[sbt_idx].data.material_data.glass.shadow_attenuation = { 0.6f, 0.6f, 0.6f };
        sbt_idx++;

        // Floor
        OPTIX_CHECK( optixSbtRecordPackHeader( state.radiance_floor_prog_group, &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.geometry_data.setParallelogram( g_floor );
        hitgroup_records[sbt_idx].data.material_data.checker       = {
            { 0.8f, 0.3f, 0.15f },   // Kd1
            { 0.9f, 0.85f, 0.05f },  // Kd2
            { 0.8f, 0.3f, 0.15f },   // Ka1
            { 0.9f, 0.85f, 0.05f },  // Ka2
            { 0.0f, 0.0f, 0.0f },    // Ks1
            { 0.0f, 0.0f, 0.0f },    // Ks2
            { 0.0f, 0.0f, 0.0f },    // Kr1
            { 0.0f, 0.0f, 0.0f },    // Kr2
            0.0f,                    // phong_exp1
            0.0f,                    // phong_exp2
            { 32.0f, 16.0f }         // inv_checker_size
        };
        sbt_idx++;

        OPTIX_CHECK( optixSbtRecordPackHeader( state.occlusion_floor_prog_group, &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.geometry_data.setParallelogram( g_floor );

        CUdeviceptr d_hitgroup_records;
        size_t      sizeof_hitgroup_record = sizeof( HitGroupRecord );
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_hitgroup_records ), sizeof_hitgroup_record * count_records ) );

        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_hitgroup_records ), hitgroup_records,
                                sizeof_hitgroup_record * count_records, cudaMemcpyHostToDevice ) );

        state.sbt.hitgroupRecordBase          = d_hitgroup_records;
        state.sbt.hitgroupRecordCount         = count_records;
        state.sbt.hitgroupRecordStrideInBytes = static_cast<uint32_t>( sizeof_hitgroup_record );
    }
}

static void context_log_cb( unsigned int level, const char* tag, const char* message, void* /*cbdata */ )
{
    std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: " << message << "\n";
}

void createContext( OptixConsoleState& state )
{
    // Initialize CUDA
    CUDA_CHECK( cudaFree( 0 ) );

    OptixDeviceContext context;
    CUcontext          cuCtx = 0;  // zero means take the current context
    OPTIX_CHECK( optixInit() );
    OptixDeviceContextOptions options = {};
    options.logCallbackFunction       = &context_log_cb;
    options.logCallbackLevel          = 4;
    OPTIX_CHECK( optixDeviceContextCreate( cuCtx, &options, &context ) );

    state.context = context;
}

void launchSubframe( OptixConsoleState& state )
{
    // Launch
    CUDA_CHECK( cudaMemcpyAsync( reinterpret_cast<void*>( state.d_params ), &state.params,
                                 sizeof( whitted::LaunchParams ), cudaMemcpyHostToDevice, state.stream ) );

    OPTIX_CHECK( optixLaunch( state.pipeline, state.stream, reinterpret_cast<CUdeviceptr>( state.d_params ),
                              sizeof( whitted::LaunchParams ), &state.sbt,
                              state.params.width,   // launch width
                              state.params.height,  // launch height
                              1                     // launch depth
                              ) );
    CUDA_SYNC_CHECK();
}

void displaySubframe( OptixConsoleState& state, std::ostream& output_stream )
{
    unsigned int width  = state.params.width;
    unsigned int height = state.params.height;

    std::vector<uchar4> output_buffer( width * height );
    CUDA_CHECK( cudaMemcpy( output_buffer.data(), state.params.frame_buffer, width * height * sizeof( uchar4 ), cudaMemcpyDeviceToHost ) );

    float              minLum = std::numeric_limits<float>::infinity();
    float              maxLum = 0;
    std::vector<float> lums( width * height );
    for( unsigned int y = 0; y < height; ++y )
    {
        uchar4* row = output_buffer.data() + ( ( height - y - 1 ) * width );
        for( unsigned int x = 0; x < width; ++x )
        {
            uchar4 ucolor = row[x];
            float3 color =
                make_float3( static_cast<float>( ucolor.x ), static_cast<float>( ucolor.y ), static_cast<float>( ucolor.z ) )
                / make_float3( 256.0f );
            float lum = color.x * 0.3f + color.y * 0.6f + color.z * 0.1f;
            minLum    = std::min( minLum, lum );
            maxLum    = std::max( maxLum, lum );

            lums[y * width + x] = lum;
        }
    }

    std::ostringstream out;
    char               lumchar[] = { ' ', '.', ',', ';', '!', 'o', '&', '8', '#', '@' };
    for( unsigned int y = 0; y < height; ++y )
    {
        for( unsigned int x = 0; x < width; ++x )
        {
            float normalized = ( lums[y * width + x] - minLum ) / ( maxLum - minLum );
            out << lumchar[static_cast<int>( normalized * 9 )];
        }
        out << "\n";
    }
    output_stream << out.str();
}

void cleanupState( OptixConsoleState& state )
{
    OPTIX_CHECK( optixPipelineDestroy( state.pipeline ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.raygen_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.radiance_metal_sphere_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.occlusion_metal_sphere_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.radiance_glass_sphere_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.occlusion_glass_sphere_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.radiance_miss_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.radiance_floor_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.occlusion_floor_prog_group ) );
    OPTIX_CHECK( optixModuleDestroy( state.shading_module ) );
    OPTIX_CHECK( optixModuleDestroy( state.geometry_module ) );
    OPTIX_CHECK( optixModuleDestroy( state.camera_module ) );
    OPTIX_CHECK( optixModuleDestroy( state.sphere_module ) );
    OPTIX_CHECK( optixDeviceContextDestroy( state.context ) );

    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.sbt.raygenRecord ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.sbt.missRecordBase ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.sbt.hitgroupRecordBase ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_gas_output_buffer ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.params.accum_buffer ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.params.frame_buffer ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.params.lights.data ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_params ) ) );
}

int main( int argc, char* argv[] )
{
    OptixConsoleState state;

    // Parse command line options
    std::ostream* out_stream = &std::cout;
    std::ofstream file_stream;
    for( int i = 1; i < argc; ++i )
    {
        const std::string arg = argv[i];
        if( arg == "--help" || arg == "-h" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "--file" || arg == "-f" )
        {
            if( i >= argc - 1 )
                printUsageAndExit( argv[0] );
            file_stream.open( argv[++i], std::ofstream::out );
            out_stream = &file_stream;
        }
        else
        {
            std::cerr << "Unknown option '" << argv[i] << "'\n";
            printUsageAndExit( argv[0] );
        }
    }

    try
    {
        // Set up OptiX state
        createContext( state );
        createGeometry( state );
        createPipeline( state );
        createSBT( state );

        // Render and display
        initLaunchParams( state );
        launchSubframe( state );
        displaySubframe( state, *out_stream );

        // Cleanup
        cleanupState( state );
    }
    catch( std::exception& e )
    {
        std::cerr << "Caught exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
