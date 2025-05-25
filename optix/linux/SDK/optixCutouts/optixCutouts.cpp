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

#include <glad/glad.h>  // Needs to be included before gl_interop

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#include <optix.h>
#include <optix_micromap.h>
#include <optix_function_table_definition.h>
#include <optix_stack_size.h>
#include <optix_stubs.h>

#include <sampleConfig.h>

#include <sutil/Camera.h>
#include <sutil/Trackball.h>
#include <sutil/CUDAOutputBuffer.h>
#include <sutil/Exception.h>
#include <sutil/GLDisplay.h>
#include <sutil/Matrix.h>
#include <sutil/sutil.h>
#include <sutil/vec_math.h>

#include <GLFW/glfw3.h>

#include "optixCutouts.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>


bool             use_pbo      = true;
bool             resize_dirty = false;
bool             minimized    = false;

// Camera state
bool             camera_changed = true;
sutil::Camera    camera;
sutil::Trackball trackball;

// Mouse state
int2             mouse_prev_pos;
int32_t          mouse_button = -1;

int32_t          samples_per_launch = 16;

//------------------------------------------------------------------------------
//
// Local types
// TODO: some of these should move to sutil or optix util header
//
//------------------------------------------------------------------------------

template <typename T>
struct Record
{
    __align__( OPTIX_SBT_RECORD_ALIGNMENT ) char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

typedef Record<RayGenData>          RayGenRecord;
typedef Record<MissData>            MissRecord;
typedef Record<CutoutsHitGroupData> HitGroupRecord;


struct Vertex
{
    float x, y, z, pad;
};


struct Instance
{
    float transform[12];
};


struct CutoutsState
{
    OptixDeviceContext context = 0;

    OptixTraversableHandle triangle_gas_handle          = 0;  // Traversable handle for triangle AS
    CUdeviceptr            d_triangle_gas_output_buffer = 0;  // Triangle AS memory
    CUdeviceptr            d_vertices                   = 0;
    CUdeviceptr            d_tex_coords                 = 0;
    CUdeviceptr            d_omm_array                  = 0; // OMM array for triangles, memory needs to be persistent over GAS build

    OptixTraversableHandle sphere_gas_handle          = 0;  // Traversable handle for sphere AS
    CUdeviceptr            d_sphere_gas_output_buffer = 0;  // Sphere AS memory

    OptixTraversableHandle ias_handle          = 0;  // Traversable handle for instance AS
    CUdeviceptr            d_ias_output_buffer = 0;  // Instance AS memory

    OptixModule module        = 0;
    OptixModule sphere_module = 0;

    OptixPipelineCompileOptions pipeline_compile_options = {};
    OptixPipeline               pipeline                 = 0;

    OptixProgramGroup raygen_prog_group               = 0;
    OptixProgramGroup radiance_miss_group             = 0;
    OptixProgramGroup occlusion_miss_group            = 0;
    OptixProgramGroup triangle_checkerboard_hit_group = 0;
    OptixProgramGroup triangle_circle_hit_group       = 0;
    OptixProgramGroup sphere_checkerboard_hit_group   = 0;


    bool enableAH   = true;
    bool enableOMMs = true;

    CUstream stream   = 0;
    Params   params   = {};
    Params*  d_params = nullptr;

    OptixShaderBindingTable sbt = {};
};


//------------------------------------------------------------------------------
//
// Scene data
//
//------------------------------------------------------------------------------


constexpr int32_t TRIANGLE_COUNT     = 32;
constexpr int32_t TRIANGLE_MAT_COUNT = 6;
constexpr int32_t SPHERE_COUNT       = 1;
constexpr int32_t SPHERE_MAT_COUNT   = 1;

// Size of the checkerboard pattern on the 'cutout' box, must be power of two for simplicity of the opacity micromap in this sample.
// The opacity micromap is generated to perfectly cover the checkerboard pattern.
constexpr int CHECKERBOARD_OMM_SUBDIV_LEVEL = 3;
constexpr int CHECKERBOARD_SIZE = 1 << CHECKERBOARD_OMM_SUBDIV_LEVEL;

constexpr int CIRCLE_OMM_SUBDIV_LEVEL = 5;

const static std::array<Vertex, TRIANGLE_COUNT*3> g_vertices =
{ {
    // Floor  -- white lambert
    {    0.0f,    0.0f,    0.0f, 0.0f },
    {    0.0f,    0.0f,  559.2f, 0.0f },
    {  556.0f,    0.0f,  559.2f, 0.0f },

    {    0.0f,    0.0f,    0.0f, 0.0f },
    {  556.0f,    0.0f,  559.2f, 0.0f },
    {  556.0f,    0.0f,    0.0f, 0.0f },

    // Ceiling -- white lambert
    {    0.0f,  548.8f,    0.0f, 0.0f },
    {  556.0f,  548.8f,    0.0f, 0.0f },
    {  556.0f,  548.8f,  559.2f, 0.0f },

    {    0.0f,  548.8f,    0.0f, 0.0f },
    {  556.0f,  548.8f,  559.2f, 0.0f },
    {    0.0f,  548.8f,  559.2f, 0.0f },

    // Back wall -- white lambert
    {    0.0f,    0.0f,  559.2f, 0.0f },
    {    0.0f,  548.8f,  559.2f, 0.0f },
    {  556.0f,  548.8f,  559.2f, 0.0f },

    {    0.0f,    0.0f,  559.2f, 0.0f },
    {  556.0f,  548.8f,  559.2f, 0.0f },
    {  556.0f,    0.0f,  559.2f, 0.0f },

    // Right wall -- green lambert
    {    0.0f,    0.0f,    0.0f, 0.0f },
    {    0.0f,  548.8f,    0.0f, 0.0f },
    {    0.0f,  548.8f,  559.2f, 0.0f },

    {    0.0f,    0.0f,    0.0f, 0.0f },
    {    0.0f,  548.8f,  559.2f, 0.0f },
    {    0.0f,    0.0f,  559.2f, 0.0f },

    // Left wall -- red lambert
    {  556.0f,    0.0f,    0.0f, 0.0f },
    {  556.0f,    0.0f,  559.2f, 0.0f },
    {  556.0f,  548.8f,  559.2f, 0.0f },

    {  556.0f,    0.0f,    0.0f, 0.0f },
    {  556.0f,  548.8f,  559.2f, 0.0f },
    {  556.0f,  548.8f,    0.0f, 0.0f },

    // Short block -- white lambert
    {  130.0f,  165.0f,   65.0f, 0.0f },
    {   82.0f,  165.0f,  225.0f, 0.0f },
    {  242.0f,  165.0f,  274.0f, 0.0f },

    {  130.0f,  165.0f,   65.0f, 0.0f },
    {  242.0f,  165.0f,  274.0f, 0.0f },
    {  290.0f,  165.0f,  114.0f, 0.0f },

    {  290.0f,    0.0f,  114.0f, 0.0f },
    {  290.0f,  165.0f,  114.0f, 0.0f },
    {  240.0f,  165.0f,  272.0f, 0.0f },

    {  290.0f,    0.0f,  114.0f, 0.0f },
    {  240.0f,  165.0f,  272.0f, 0.0f },
    {  240.0f,    0.0f,  272.0f, 0.0f },

    {  130.0f,    0.0f,   65.0f, 0.0f },
    {  130.0f,  165.0f,   65.0f, 0.0f },
    {  290.0f,  165.0f,  114.0f, 0.0f },

    {  130.0f,    0.0f,   65.0f, 0.0f },
    {  290.0f,  165.0f,  114.0f, 0.0f },
    {  290.0f,    0.0f,  114.0f, 0.0f },

    {   82.0f,    0.0f,  225.0f, 0.0f },
    {   82.0f,  165.0f,  225.0f, 0.0f },
    {  130.0f,  165.0f,   65.0f, 0.0f },

    {   82.0f,    0.0f,  225.0f, 0.0f },
    {  130.0f,  165.0f,   65.0f, 0.0f },
    {  130.0f,    0.0f,   65.0f, 0.0f },

    {  240.0f,    0.0f,  272.0f, 0.0f },
    {  240.0f,  165.0f,  272.0f, 0.0f },
    {   82.0f,  165.0f,  225.0f, 0.0f },

    {  240.0f,    0.0f,  272.0f, 0.0f },
    {   82.0f,  165.0f,  225.0f, 0.0f },
    {   82.0f,    0.0f,  225.0f, 0.0f },

    // Tall block -- white lambert
    {  423.0f,  330.0f,  247.0f, 0.0f },
    {  265.0f,  330.0f,  296.0f, 0.0f },
    {  314.0f,  330.0f,  455.0f, 0.0f },

    {  423.0f,  330.0f,  247.0f, 0.0f },
    {  314.0f,  330.0f,  455.0f, 0.0f },
    {  472.0f,  330.0f,  406.0f, 0.0f },

    {  423.0f,    0.0f,  247.0f, 0.0f },
    {  423.0f,  330.0f,  247.0f, 0.0f },
    {  472.0f,  330.0f,  406.0f, 0.0f },

    {  423.0f,    0.0f,  247.0f, 0.0f },
    {  472.0f,  330.0f,  406.0f, 0.0f },
    {  472.0f,    0.0f,  406.0f, 0.0f },

    {  472.0f,    0.0f,  406.0f, 0.0f },
    {  472.0f,  330.0f,  406.0f, 0.0f },
    {  314.0f,  330.0f,  456.0f, 0.0f },

    {  472.0f,    0.0f,  406.0f, 0.0f },
    {  314.0f,  330.0f,  456.0f, 0.0f },
    {  314.0f,    0.0f,  456.0f, 0.0f },

    {  314.0f,    0.0f,  456.0f, 0.0f },
    {  314.0f,  330.0f,  456.0f, 0.0f },
    {  265.0f,  330.0f,  296.0f, 0.0f },

    {  314.0f,    0.0f,  456.0f, 0.0f },
    {  265.0f,  330.0f,  296.0f, 0.0f },
    {  265.0f,    0.0f,  296.0f, 0.0f },

    {  265.0f,    0.0f,  296.0f, 0.0f },
    {  265.0f,  330.0f,  296.0f, 0.0f },
    {  423.0f,  330.0f,  247.0f, 0.0f },

    {  265.0f,    0.0f,  296.0f, 0.0f },
    {  423.0f,  330.0f,  247.0f, 0.0f },
    {  423.0f,    0.0f,  247.0f, 0.0f },

    // Ceiling light -- emmissive
    {  343.0f,  548.6f,  227.0f, 0.0f },
    {  213.0f,  548.6f,  227.0f, 0.0f },
    {  213.0f,  548.6f,  332.0f, 0.0f },

    {  343.0f,  548.6f,  227.0f, 0.0f },
    {  213.0f,  548.6f,  332.0f, 0.0f },
    {  343.0f,  548.6f,  332.0f, 0.0f }
} };


static std::array<uint32_t, TRIANGLE_COUNT> g_mat_indices =
{ {
    0, 0,                          // Floor         -- white lambert
    0, 0,                          // Ceiling       -- white lambert
    0, 0,                          // Back wall     -- white lambert
    1, 1,                          // Right wall    -- green lambert
    2, 2,                          // Left wall     -- red lambert
    4, 4, 4, 4, 5, 5, 4, 4, 5, 5,  // Short block   -- cutout checkerboard and circle
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // Tall block    -- white lambert
    3, 3                           // Ceiling light -- emmissive
} };


const std::array<float3, TRIANGLE_MAT_COUNT> g_emission_colors =
{ {
    {  0.0f,  0.0f, 0.0f },
    {  0.0f,  0.0f, 0.0f },
    {  0.0f,  0.0f, 0.0f },
    { 15.0f, 15.0f, 5.0f },
    {  0.0f,  0.0f, 0.0f },
    {  0.0f,  0.0f, 0.0f }
} };


const std::array<float3, TRIANGLE_MAT_COUNT> g_diffuse_colors =
{ {
    { 0.80f, 0.80f, 0.80f },
    { 0.05f, 0.80f, 0.05f },
    { 0.80f, 0.05f, 0.05f },
    { 0.50f, 0.00f, 0.00f },
    { 0.70f, 0.25f, 0.00f },
    { 0.70f, 0.25f, 0.00f }
} };

const std::array<float2, 2 * 3> g_checkerboard_tex_coords =
{ {
    { CHECKERBOARD_SIZE, 0.0f }, { 0.0f, 0.0f },
    { 0.0f, CHECKERBOARD_SIZE }, { CHECKERBOARD_SIZE, 0.0f },
    { 0.0f, CHECKERBOARD_SIZE }, { CHECKERBOARD_SIZE, CHECKERBOARD_SIZE }
} };

const std::array<float2, 2 * 3> g_circle_tex_coords =
{ {
    { 1.f, -1.0f }, { -1.0f, -1.0f },
    { -1.0f, 1.f }, { 1.f, -1.0f },
    { -1.0f, 1.f }, { 1.f, 1.f }
} };

// NB: Some UV scaling is baked into the coordinates for the short block, since
//     the coordinates are used for the cutout texture.
const std::array<float2, TRIANGLE_COUNT* 3> g_tex_coords =
{ {
    // Floor
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f },

    // Ceiling
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f },

    // Back wall
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f },

    // Right wall
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f },

    // Left wall
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f },

    // Short Block
    g_checkerboard_tex_coords[0], g_checkerboard_tex_coords[1], g_checkerboard_tex_coords[2],
    g_checkerboard_tex_coords[3], g_checkerboard_tex_coords[4], g_checkerboard_tex_coords[5],
    g_checkerboard_tex_coords[0], g_checkerboard_tex_coords[1], g_checkerboard_tex_coords[2],
    g_checkerboard_tex_coords[3], g_checkerboard_tex_coords[4], g_checkerboard_tex_coords[5],
    g_circle_tex_coords[0], g_circle_tex_coords[1], g_circle_tex_coords[2],
    g_circle_tex_coords[3], g_circle_tex_coords[4], g_circle_tex_coords[5],
    g_checkerboard_tex_coords[0], g_checkerboard_tex_coords[1], g_checkerboard_tex_coords[2],
    g_checkerboard_tex_coords[3], g_checkerboard_tex_coords[4], g_checkerboard_tex_coords[5],
    g_circle_tex_coords[0], g_circle_tex_coords[1], g_circle_tex_coords[2],
    g_circle_tex_coords[3], g_circle_tex_coords[4], g_circle_tex_coords[5],

    // Tall Block
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f },
    { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
    { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f },
    { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
    { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f },
    { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
    { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f },
    { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f },
    { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f },

    // Ceiling light
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f },
    { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }
} };


const GeometryData::Sphere g_sphere                = {410.0f, 90.0f, 110.0f, 90.0f};
const float3               g_sphere_emission_color = {0.0f};
const float3               g_sphere_diffuse_color  = {0.1f, 0.2f, 0.8f};

// decl
void buildInstanceAccel( CutoutsState& state );

//------------------------------------------------------------------------------
//
// GLFW callbacks
//
//------------------------------------------------------------------------------

static void mouseButtonCallback( GLFWwindow* window, int button, int action, int mods )
{
    double xpos, ypos;
    glfwGetCursorPos( window, &xpos, &ypos );

    if( action == GLFW_PRESS )
    {
        mouse_button = button;
        trackball.startTracking(static_cast<int>( xpos ), static_cast<int>( ypos ));
    }
    else
    {
        mouse_button = -1;
    }
}


static void cursorPosCallback( GLFWwindow* window, double xpos, double ypos )
{
    CutoutsState& state  = *static_cast<CutoutsState*>( glfwGetWindowUserPointer( window ) );
    Params&       params = state.params;

    if( mouse_button == GLFW_MOUSE_BUTTON_LEFT )
    {
        trackball.setViewMode( sutil::Trackball::LookAtFixed );
        trackball.updateTracking( static_cast<int>( xpos ), static_cast<int>( ypos ), params.width, params.height );
        camera_changed = true;
    }
    else if( mouse_button == GLFW_MOUSE_BUTTON_RIGHT )
    {
        trackball.setViewMode( sutil::Trackball::EyeFixed );
        trackball.updateTracking( static_cast<int>( xpos ), static_cast<int>( ypos ), params.width, params.height );
        camera_changed = true;
    }
}


static void windowSizeCallback( GLFWwindow* window, int32_t res_x, int32_t res_y )
{
    CutoutsState& state = *static_cast<CutoutsState*>( glfwGetWindowUserPointer( window ) );
    Params&       params = state.params;
    // Keep rendering at the current resolution when the window is minimized.
    if( minimized )
        return;

    // Output dimensions must be at least 1 in both x and y.
    sutil::ensureMinimumSize( res_x, res_y );

    params.width  = res_x;
    params.height = res_y;
    camera_changed = true;
    resize_dirty   = true;
}


static void windowIconifyCallback( GLFWwindow* window, int32_t iconified )
{
    minimized = ( iconified > 0 );
}


static void keyCallback( GLFWwindow* window, int32_t key, int32_t /*scancode*/, int32_t action, int32_t /*mods*/ )
{
    CutoutsState& state = *static_cast<CutoutsState*>( glfwGetWindowUserPointer( window ) );

    if( action == GLFW_PRESS )
    {
        if( key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE )
        {
            glfwSetWindowShouldClose( window, true );
        }
    }
    else if( key == GLFW_KEY_O )
    {
        // toggle enabling/disabling OMMs
        // in this sample we always add OMMs to the triangles
        // we can disable OMMs at the instance level (using instance flag OPTIX_INSTANCE_FLAG_DISABLE_OPACITY_MICROMAPS)
        state.enableOMMs = !state.enableOMMs;
        buildInstanceAccel( state );
        state.params.subframe_index = 0;
        if( state.enableOMMs )
            std::cout << "Opacity micromaps (OMMs) on small block enabled.\n";
        else
            std::cout << "Opacity micromaps (OMMs) on small block disabled.\n";

    }
    else if( key == GLFW_KEY_A )
    {
        // toggle enabling/disabling AH program (entirely rely on OMMs)
        // Like OMMs, AH can be disabled at the instance level (using instance flag OPTIX_INSTANCE_FLAG_DISABLE_ANYHIT)
        state.enableAH = !state.enableAH;
        buildInstanceAccel( state );
        state.params.subframe_index = 0;
        if( state.enableAH )
            std::cout << "Anyhit program (AH) on small block enabled.\n";
        else
            std::cout << "Anyhit program (AH) on small block disabled.\n";
    }
}


static void scrollCallback( GLFWwindow* window, double xscroll, double yscroll )
{
    if(trackball.wheelEvent((int)yscroll))
        camera_changed = true;
}


//------------------------------------------------------------------------------
//
// Helper functions
// TODO: some of these should move to sutil or optix util header
//
//------------------------------------------------------------------------------

void printUsageAndExit( const char* argv0 )
{
    std::cerr << "Usage  : " << argv0 << " [options]\n";
    std::cerr << "Options: --file | -f <filename>      File for image output\n";
    std::cerr << "         --launch-samples | -s       Number of samples per pixel per launch (default 16)\n";
    std::cerr << "         --no-gl-interop             Disable GL interop for display\n";
    std::cerr << "         --dim=<width>x<height>      Set image dimensions; defaults to 768x768\n";
    std::cerr << "         --help | -h                 Print this usage message\n";
    exit( 0 );
}


void initLaunchParams( CutoutsState& state )
{
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.params.accum_buffer ),
                            state.params.width*state.params.height*sizeof(float4) ) );
    state.params.frame_buffer = nullptr; // Will be set when output buffer is mapped

    state.params.samples_per_launch = samples_per_launch;
    state.params.subframe_index = 0u;

    state.params.light.emission = make_float3(   15.0f,  15.0f,   5.0f );
    state.params.light.corner   = make_float3(  343.0f, 548.5f, 227.0f );
    state.params.light.v1       = make_float3(    0.0f,   0.0f, 105.0f );
    state.params.light.v2       = make_float3( -130.0f,   0.0f,   0.0f );
    state.params.light.normal   = normalize  ( cross( state.params.light.v1,  state.params.light.v2) );

    CUDA_CHECK( cudaStreamCreate( &state.stream ) );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_params ), sizeof( Params ) ) );
}


void handleCameraUpdate( Params& params )
{
    if( !camera_changed )
        return;
    camera_changed = false;

    camera.setAspectRatio( static_cast<float>( params.width ) / static_cast<float>( params.height ) );
    params.eye = camera.eye();
    camera.UVWFrame( params.U, params.V, params.W );
}


void handleResize( sutil::CUDAOutputBuffer<uchar4>& output_buffer, Params& params )
{
    if( !resize_dirty )
        return;
    resize_dirty = false;

    output_buffer.resize( params.width, params.height );

    // Realloc accumulation buffer
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( params.accum_buffer ) ) );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &params.accum_buffer ),
                            params.width*params.height*sizeof(float4) ) );
}


void updateState( sutil::CUDAOutputBuffer<uchar4>& output_buffer, Params& params )
{
    // Update params on device
    if( camera_changed || resize_dirty )
        params.subframe_index = 0;

    handleCameraUpdate( params );
    handleResize( output_buffer, params );
}


void launchSubframe( sutil::CUDAOutputBuffer<uchar4>& output_buffer, CutoutsState& state )
{
    // Launch
    uchar4* result_buffer_data = output_buffer.map();
    state.params.frame_buffer = result_buffer_data;
    CUDA_CHECK( cudaMemcpyAsync( reinterpret_cast<void*>( state.d_params ),
                &state.params,
                sizeof( Params ),
                cudaMemcpyHostToDevice,
                state.stream
                ) );

    OPTIX_CHECK( optixLaunch(
                 state.pipeline,
                 state.stream,
                 reinterpret_cast<CUdeviceptr>( state.d_params ),
                 sizeof( Params ),
                 &state.sbt,
                 state.params.width,  // launch width
                 state.params.height, // launch height
                 1                    // launch depth
                 ) );
    output_buffer.unmap();
}


void displaySubframe(
        sutil::CUDAOutputBuffer<uchar4>&  output_buffer,
        sutil::GLDisplay&                 gl_display,
        GLFWwindow*                       window )
{
    // Display
    int framebuf_res_x = 0;   // The display's resolution (could be HDPI res)
    int framebuf_res_y = 0;   //
    glfwGetFramebufferSize( window, &framebuf_res_x, &framebuf_res_y );
    gl_display.display(
            output_buffer.width(),
            output_buffer.height(),
            framebuf_res_x,
            framebuf_res_y,
            output_buffer.getPBO()
            );
}


static void context_log_cb( unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: "
              << message << "\n";
}


void initCameraState()
{
    camera.setEye( make_float3( 278.0f, 273.0f, -900.0f ) );
    camera.setLookat( make_float3( 278.0f, 273.0f, 330.0f ) );
    camera.setUp( make_float3( 0.0f, 1.0f, 0.0f ) );
    camera.setFovY( 35.0f );
    camera_changed = true;

    trackball.setCamera( &camera );
    trackball.setMoveSpeed( 10.0f );
    trackball.setReferenceFrame( make_float3( 1.0f, 0.0f, 0.0f ),
                                 make_float3( 0.0f, 0.0f, 1.0f ),
                                 make_float3( 0.0f, 1.0f, 0.0f ) );
    trackball.setGimbalLock(true);
}


void createContext( CutoutsState& state )
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


void buildGeomAccel( CutoutsState& state )
{
    //
    // Build triangle GAS
    //
    {
        const size_t vertices_size_in_bytes = g_vertices.size() * sizeof( Vertex );
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_vertices ), vertices_size_in_bytes ) );
        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( state.d_vertices ), g_vertices.data(), vertices_size_in_bytes,
                                cudaMemcpyHostToDevice ) );

        CUdeviceptr  d_mat_indices             = 0;
        const size_t mat_indices_size_in_bytes = g_mat_indices.size() * sizeof( uint32_t );
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_mat_indices ), mat_indices_size_in_bytes ) );
        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_mat_indices ), g_mat_indices.data(),
                                mat_indices_size_in_bytes, cudaMemcpyHostToDevice ) );

        // NOTE: the 'DISABLE_ANYHIT' flag will be overwritten by the explicit OMM predefined index below.
        // With OMMs, the opacity state is explicitly defined per triangle.
        uint32_t triangle_input_flags[TRIANGLE_MAT_COUNT] = {
            // One flag per SBT record for this build input
            // The following materials are known to be opaque, so normally, we would use OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT.
            // However, the usage of OMMs in the AS build input overwrites flag OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT.
            // Instead we use predefined OMM indices to mark the triangles opaque that make use of opaque materials.
            OPTIX_GEOMETRY_FLAG_NONE, // opaque material
            OPTIX_GEOMETRY_FLAG_NONE, // opaque material
            OPTIX_GEOMETRY_FLAG_NONE, // opaque material
            OPTIX_GEOMETRY_FLAG_NONE, // opaque material
            OPTIX_GEOMETRY_FLAG_NONE  // cutout material
        };

        std::array<OptixOpacityMicromapUsageCount, 2> ommUsages ={};
        {
            OptixOpacityMicromapUsageCount& ommUsageCheckerboard = ommUsages[0];
            ommUsageCheckerboard.count = 6;  // 3 out of 5 sides of a box, 2 triangles per side reference an OMM in the OMM array
            ommUsageCheckerboard.format           = OPTIX_OPACITY_MICROMAP_FORMAT_2_STATE;  // simple 2 state as the OMM perfectly matches the checkerboard pattern. 'unknown' states that are resolved in the anyhit program are not needed.
            ommUsageCheckerboard.subdivisionLevel = CHECKERBOARD_OMM_SUBDIV_LEVEL;
        }
        {
            OptixOpacityMicromapUsageCount& ommUsageCirle = ommUsages[1];
            ommUsageCirle.count = 4;  // 2 out of 5 sides of a box, 2 triangles per side reference an OMM in the OMM array
            ommUsageCirle.format = OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE;  // 4 state, some parts need to be resolved in the anyhit program.
            ommUsageCirle.subdivisionLevel = CIRCLE_OMM_SUBDIV_LEVEL;
        }

        OptixBuildInputOpacityMicromap ommInput ={};
        ommInput.opacityMicromapArray = state.d_omm_array;
        ommInput.indexingMode = OPTIX_OPACITY_MICROMAP_ARRAY_INDEXING_MODE_INDEXED;
        ommInput.indexSizeInBytes = 2;
        ommInput.numMicromapUsageCounts = static_cast<unsigned int>(ommUsages.size());
        ommInput.micromapUsageCounts = ommUsages.data();

        // OMM indexing must be specified for all triangles in this build input.
        // Since only the triangles of the small box actually reference OMMs, predefined indices must be used for the other triangles.
        // Alternatively, a separate build input can be used for the geometry that uses OMMs (the small box)
        // and only that build input references the OMM array.
        constexpr unsigned int numTriangles = static_cast<uint32_t>(g_vertices.size()) / 3;
        constexpr unsigned short opaqueIndex = static_cast<unsigned short>( OPTIX_OPACITY_MICROMAP_PREDEFINED_INDEX_FULLY_OPAQUE );
        std::array<unsigned short, numTriangles> ommIndices ={
            opaqueIndex, opaqueIndex, // floor
            opaqueIndex, opaqueIndex, // ceiling
            opaqueIndex, opaqueIndex, // back wall
            opaqueIndex, opaqueIndex, // right wall
            opaqueIndex, opaqueIndex, // left wall
            0, 1, 0, 1, 2, 3, 0, 1, 2, 3,  // small box, three sides use OMMs 0 and 1, two sides of the box (front/back) use OMMs 2 and 3
            opaqueIndex, opaqueIndex, // tall box ...
            opaqueIndex, opaqueIndex,
            opaqueIndex, opaqueIndex,
            opaqueIndex, opaqueIndex,
            opaqueIndex, opaqueIndex,
            opaqueIndex, opaqueIndex  // ceiling light
        };
        const size_t omm_indices_size_in_bytes = ommIndices.size() * ommInput.indexSizeInBytes;
        CUdeviceptr  d_omm_indices             = 0;
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_omm_indices ), omm_indices_size_in_bytes ) );
        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_omm_indices ), ommIndices.data(),
                                omm_indices_size_in_bytes, cudaMemcpyHostToDevice ) );
        ommInput.indexBuffer = d_omm_indices;

        OptixBuildInput triangle_input                           = {};
        triangle_input.type                                      = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
        triangle_input.triangleArray.vertexFormat                = OPTIX_VERTEX_FORMAT_FLOAT3;
        triangle_input.triangleArray.vertexStrideInBytes         = sizeof( Vertex );
        triangle_input.triangleArray.numVertices                 = static_cast<uint32_t>( g_vertices.size() );
        triangle_input.triangleArray.vertexBuffers               = &state.d_vertices;
        triangle_input.triangleArray.flags                       = triangle_input_flags;
        triangle_input.triangleArray.numSbtRecords               = TRIANGLE_MAT_COUNT;
        triangle_input.triangleArray.sbtIndexOffsetBuffer        = d_mat_indices;
        triangle_input.triangleArray.sbtIndexOffsetSizeInBytes   = sizeof( uint32_t );
        triangle_input.triangleArray.sbtIndexOffsetStrideInBytes = sizeof( uint32_t );

        triangle_input.triangleArray.opacityMicromap = ommInput;

        OptixAccelBuildOptions accel_options = {};
        // Enable 'OPTIX_BUILD_FLAG_ALLOW_DISABLE_OPACITY_MICROMAPS' for demonstration purposes to allow for toggling between enabling/disabling OMMs at runtime quickly
        // we toggle by disabling OMMs at the instance level (using instance flag OPTIX_INSTANCE_FLAG_DISABLE_OPACITY_MICROMAPS)
        accel_options.buildFlags             = OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_ALLOW_DISABLE_OPACITY_MICROMAPS;
        accel_options.operation              = OPTIX_BUILD_OPERATION_BUILD;

        OptixAccelBufferSizes gas_buffer_sizes;
        OPTIX_CHECK( optixAccelComputeMemoryUsage( state.context, &accel_options, &triangle_input,
                                                   1,  // num_build_inputs
                                                   &gas_buffer_sizes ) );

        CUdeviceptr d_temp_buffer;
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_temp_buffer ), gas_buffer_sizes.tempSizeInBytes ) );

        // non-compacted output
        CUdeviceptr d_buffer_temp_output_gas_and_compacted_size;
        size_t      compactedSizeOffset = roundUp<size_t>( gas_buffer_sizes.outputSizeInBytes, 8ull );
        CUDA_CHECK( cudaMalloc(
                    reinterpret_cast<void**>( &d_buffer_temp_output_gas_and_compacted_size ),
                    compactedSizeOffset + 8
                    ) );

        OptixAccelEmitDesc emitProperty = {};
        emitProperty.type               = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
        emitProperty.result = ( CUdeviceptr )( (char*)d_buffer_temp_output_gas_and_compacted_size + compactedSizeOffset );

        OPTIX_CHECK( optixAccelBuild(
                    state.context,
                    0,              // CUDA stream
                    &accel_options,
                    &triangle_input,
                    1,              // num build inputs
                    d_temp_buffer,
                    gas_buffer_sizes.tempSizeInBytes,
                    d_buffer_temp_output_gas_and_compacted_size,
                    gas_buffer_sizes.outputSizeInBytes,
                    &state.triangle_gas_handle,
                    &emitProperty,  // emitted property list
                    1               // num emitted properties
                    ) );

        CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_temp_buffer ) ) );
        CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_mat_indices ) ) );
        CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_omm_indices ) ) );

        size_t compacted_gas_size;
        CUDA_CHECK( cudaMemcpy(
                    &compacted_gas_size,
                    (void*)emitProperty.result,
                    sizeof( size_t ),
                    cudaMemcpyDeviceToHost
                    ) );

        if( compacted_gas_size < gas_buffer_sizes.outputSizeInBytes )
        {
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_triangle_gas_output_buffer ), compacted_gas_size ) );

            // use handle as input and output
            OPTIX_CHECK( optixAccelCompact( state.context, 0, state.triangle_gas_handle, state.d_triangle_gas_output_buffer, compacted_gas_size, &state.triangle_gas_handle ) );

            CUDA_CHECK( cudaFree( (void*)d_buffer_temp_output_gas_and_compacted_size ) );
        }
        else
        {
            state.d_triangle_gas_output_buffer = d_buffer_temp_output_gas_and_compacted_size;
        }
    }

    //
    // Build sphere GAS
    //
    {
        OptixAccelBuildOptions accel_options = {};
        accel_options.buildFlags             = OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
        accel_options.operation              = OPTIX_BUILD_OPERATION_BUILD;

        // AABB build input
        float3    m_min = g_sphere.center - g_sphere.radius;
        float3    m_max = g_sphere.center + g_sphere.radius;
        OptixAabb aabb  = {m_min.x, m_min.y, m_min.z, m_max.x, m_max.y, m_max.z};

        CUdeviceptr d_aabb_buffer;
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_aabb_buffer ), sizeof( OptixAabb ) ) );
        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_aabb_buffer ), &aabb, sizeof( OptixAabb ),
                                cudaMemcpyHostToDevice ) );

        uint32_t sphere_input_flag = OPTIX_GEOMETRY_FLAG_NONE;
        OptixBuildInput sphere_input                    = {};
        sphere_input.type                               = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
        sphere_input.customPrimitiveArray.aabbBuffers   = &d_aabb_buffer;
        sphere_input.customPrimitiveArray.numPrimitives = 1;
        sphere_input.customPrimitiveArray.flags         = &sphere_input_flag;
        sphere_input.customPrimitiveArray.numSbtRecords = 1;

        OptixAccelBufferSizes gas_buffer_sizes;
        OPTIX_CHECK( optixAccelComputeMemoryUsage( state.context,
                                                   &accel_options,
                                                   &sphere_input,
                                                   1,  // num_build_inputs
                                                   &gas_buffer_sizes ) );

        CUdeviceptr d_temp_buffer;
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_temp_buffer ), gas_buffer_sizes.tempSizeInBytes ) );

        // non-compacted output
        CUdeviceptr d_buffer_temp_output_gas_and_compacted_size;
        size_t      compactedSizeOffset = roundUp<size_t>( gas_buffer_sizes.outputSizeInBytes, 8ull );
        CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_buffer_temp_output_gas_and_compacted_size ), compactedSizeOffset + 8 ) );

        OptixAccelEmitDesc emitProperty = {};
        emitProperty.type               = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
        emitProperty.result = ( CUdeviceptr )( (char*)d_buffer_temp_output_gas_and_compacted_size + compactedSizeOffset );

        OPTIX_CHECK( optixAccelBuild( state.context,
                                      0,        // CUDA stream
                                      &accel_options,
                                      &sphere_input,
                                      1,        // num build inputs
                                      d_temp_buffer,
                                      gas_buffer_sizes.tempSizeInBytes,
                                      d_buffer_temp_output_gas_and_compacted_size,
                                      gas_buffer_sizes.outputSizeInBytes,
                                      &state.sphere_gas_handle,
                                      &emitProperty,  // emitted property list
                                      1 ) );          // num emitted properties

        CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_temp_buffer ) ) );
        CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_aabb_buffer ) ) );

        size_t compacted_gas_size;
        CUDA_CHECK( cudaMemcpy( &compacted_gas_size, (void*)emitProperty.result, sizeof( size_t ), cudaMemcpyDeviceToHost ) );

        if( compacted_gas_size < gas_buffer_sizes.outputSizeInBytes )
        {
            CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_sphere_gas_output_buffer ), compacted_gas_size ) );

            // use handle as input and output
            OPTIX_CHECK( optixAccelCompact( state.context, 0, state.sphere_gas_handle, state.d_sphere_gas_output_buffer, compacted_gas_size, &state.sphere_gas_handle ) );

            CUDA_CHECK( cudaFree( (void*)d_buffer_temp_output_gas_and_compacted_size ) );
        }
        else
        {
            state.d_sphere_gas_output_buffer = d_buffer_temp_output_gas_and_compacted_size;
        }
    }
}

void buildCheckerboardOpacityMicromap( CutoutsState& state )
{
    // Need two histogram entries, one for the checkerboard pattern (combination of OMM format and OMM subdivision level),
    // and one for the circular pattern
    std::array<OptixOpacityMicromapHistogramEntry, 2> histogram;

    {
        OptixOpacityMicromapHistogramEntry& entry = histogram[0];
        entry.count                               = 2;
        entry.format                              = OptixOpacityMicromapFormat::OPTIX_OPACITY_MICROMAP_FORMAT_2_STATE;
        entry.subdivisionLevel                    = CHECKERBOARD_OMM_SUBDIV_LEVEL;
    }
    {
        OptixOpacityMicromapHistogramEntry& entry = histogram[1];
        entry.count                               = 2;
        entry.format                              = OptixOpacityMicromapFormat::OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE;
        entry.subdivisionLevel                    = CIRCLE_OMM_SUBDIV_LEVEL;
    }

    constexpr int numCheckerboardMicroTriangles = 1 << ( CHECKERBOARD_OMM_SUBDIV_LEVEL * 2 );
    std::array<std::array<unsigned short, numCheckerboardMicroTriangles / 16>, 2> ommDataCheckerboard ={}; // 2 OMMs

    constexpr int numCircleMicroTriangles = 1 << ( CIRCLE_OMM_SUBDIV_LEVEL * 2 );
    std::array<std::array<unsigned short, numCircleMicroTriangles * 2 / 16>, 2> ommDataCircle = {};  // 2 OMMs with 2b per state

    CUdeviceptr  d_omm_input_data = 0;
    const size_t omm_data_checkerboard_size_in_bytes = numCheckerboardMicroTriangles / 8 * 2;  // 2 OMMs, 1b per micro triangle
    const size_t omm_data_circle_size_in_bytes = numCircleMicroTriangles / 8 * 2 * 2;  // 2 OMMs, 2b per micro triangle
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_omm_input_data ), omm_data_checkerboard_size_in_bytes + omm_data_circle_size_in_bytes ) );

    auto computeUV = []( const float2& bary, const float2* texcoord )
    {
        return ( 1 - bary.x - bary.y ) * texcoord[0] + bary.x * texcoord[1] + bary.y * texcoord[2];
    };
    {
        // OMMs are used for 'quads' with vertices a,b,c,d as:
        // first triangle:  a, b, c
        // second triangle: a, c, d

#if 0
        // Knowing the order of the micro triangles, it is possible to hard code the sequence for a checkerboard pattern

        // The simplified, 16 microtriangles example pictured below illustrates the space filling curve.
        //
        //                  w                            |
        //                 / \                           |
        //                v   \                          |
        //                     x---                      |
        //                \   / \                        |
        //                 \ / F \                       |
        //                  ----------                   |
        //             \   / \ D / \                     |
        //              \ / E \ / C \                    |
        //               ----------------                |
        //          \   / \ 4 / \ 6 / \                  |
        //           \ / 3 \ / 5 \ / B \                 |
        //            ----------------------             |
        //       \   / \ 1 / \ 7 / \ 9 / \    ^          |
        //        \ / 0 \ / 2 \ / 8 \ / A \    \         |
        //         x-----------------------x--- v        |
        //        /     /     /     /     /              |
        //       u - >                                   |

        unsigned short t00 = 0b1100100111001001; // 1100 1001 1100 1001
        unsigned short t01 = 0b1001110010011100; // 1001 1100 1001 1100
        unsigned short t10 = 0b0101010101010101; // 0101 0101 0101 0101
        for( size_t i = 0; i < ommData[0].size(); ++i )
        {
            ommData[0][i] = i&1 ? t01 : t00;
        }
        for( size_t i = 0; i < ommData[1].size(); ++i )
        {
            ommData[1][i] = t10;
        }
#else
        // Alternatively, one can use the uv values of the triangles to determine the position of the micro triangle and determine the state
        // Note that the tex coords (uvs) are in range [0, CHECKERBOARD_SIZE] for the checkerboard in this sample.
        ommDataCheckerboard[0].fill( 0 );
        ommDataCheckerboard[1].fill( 0 );
        const float2* tex_coords_t0 = &g_checkerboard_tex_coords[0];
        const float2* tex_coords_t1 = &g_checkerboard_tex_coords[3];
        for( uint32_t uTriI=0; uTriI<numCheckerboardMicroTriangles; ++uTriI )
        {
            float2 bary0, bary1, bary2;
            optixMicromapIndexToBaseBarycentrics( uTriI, CHECKERBOARD_OMM_SUBDIV_LEVEL, bary0, bary1, bary2 );
            constexpr float oneThird = 1.f / 3.f;
            float2          midbary  = oneThird * bary0 + oneThird * bary1 + oneThird * bary2;
            {
                // first triangle (a,b,c)
                // compute barycentrics of the midpoint of the micro triangle
                float2 uvMidPoint = computeUV(midbary, tex_coords_t0);
                // using the OMM state values: ((int( uvMidPoint.x ) & 1) == (int( uvMidPoint.y ) & 1)) ? OPTIX_OPACITY_MICROMAP_STATE_OPAQUE : OPTIX_OPACITY_MICROMAP_STATE_TRANSPARENT)
                // using the bit directly (since OPTIX_OPACITY_MICROMAP_STATE_OPAQUE == 1):
                ommDataCheckerboard[0][uTriI / 16] |= ((int( uvMidPoint.x ) & 1) == (int( uvMidPoint.y ) & 1)) << (uTriI % 16);  // set opaque if the 'integer' uvs are equal
            }
            {
                // second triangle (a,c,d)
                float2 uvMidPoint = computeUV( midbary, tex_coords_t1 );
                ommDataCheckerboard[1][uTriI / 16] |= ((int( uvMidPoint.x ) & 1) == (int( uvMidPoint.y ) & 1)) << (uTriI % 16);  // set opaque if the 'integer' uvs are equal
            }
        }
#endif

        CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_omm_input_data ), ommDataCheckerboard.data(),
                                omm_data_checkerboard_size_in_bytes, cudaMemcpyHostToDevice ) );
    }

    {
        // Use the uv values of the triangles to determine the position of the micro triangle and determine the state
        // Note that the tex coords (uvs) are in range [-1,1] for the circular cutout.
        ommDataCircle[0].fill( 0 );
        ommDataCircle[1].fill( 0 );
        const float2* tex_coords_t0 = &g_circle_tex_coords[0];
        const float2* tex_coords_t1 = &g_circle_tex_coords[3];
        for( uint32_t uTriI = 0; uTriI < numCircleMicroTriangles; ++uTriI )
        {
            // Opacity micromaps for a circular cutout.
            // Note that this computation is assumed to align with the anyhit program (AH).
            // While AH only needs to evaluate opacity of a single (intersection) point, in the following we must determine the
            // opacity state of the micro triangles, i.e., an area, making the evaluation more involved:
            // Check if the micro triangle overlaps the circle, if so, it needs to be marked as 'unknown'.
            // If the micro triangle is fully within the circle, it is marked as 'transparent'.
            // Otherwise it must be fully outside the circle and can be marked 'opaque'.

            // AH:
            // texcoord = t0 * ( 1.0f - barycentrics.x - barycentrics.y ) + t1 * barycentrics.x + t2 * barycentrics.y;
            // ignore   = ( texcoord.x * texcoord.x + texcoord.y * texcoord.y ) < ( CIRCLE_RADIUS * CIRCLE_RADIUS );

            auto inCircle = [&]( const float2& uv ) -> bool
            {
                // check if point uv is in circle with center at [0,0] and radius CIRCLE_RADIUS
                return ( uv.x * uv.x + uv.y * uv.y ) < ( CIRCLE_RADIUS * CIRCLE_RADIUS );
            };
            auto edgeIntersectsCircle = [&]( const float2& uv0, const float2& uv1 ) -> bool
            {
                float2 d = uv1 - uv0;
                float2 f = uv0; // circle center is at [0,0]

                float a = dot( d, d );
                float b = 2.f * dot( f, d );
                float c = dot(f, f) - CIRCLE_RADIUS * CIRCLE_RADIUS;

                float discriminant = b * b - 4 * a * c;
                if( discriminant < 0 )
                {
                    // no intersection
                    return false;
                }
                else
                {
                    // there is a solution to the equation.
                    discriminant = sqrtf( discriminant );

                    float t0 = ( -b - discriminant ) / ( 2.f * a );
                    float t1 = ( -b + discriminant ) / ( 2.f * a );

                    // check for solutions of the quadratic equation, must be in range [0,1] to be 'within the edge'
                    if( (t0 >= 0 && t0 <= 1.f) || (t1 >= 0 && t1 <= 1))
                    {
                        return true;
                    }

                    // no intersection: fully in front of, behind, or inside the circe
                    return false;
                }
            };

            float2 bary0, bary1, bary2;
            optixMicromapIndexToBaseBarycentrics( uTriI, CIRCLE_OMM_SUBDIV_LEVEL, bary0, bary1, bary2 );

            auto computeOMMData = [&]( int ommIdx, const float2* tex_coords )
            {
                float2 uv0         = computeUV( bary0, tex_coords );
                float2 uv1         = computeUV( bary1, tex_coords );
                float2 uv2         = computeUV( bary2, tex_coords );
                bool   isInCircle0 = inCircle( uv0 );
                bool   isInCircle1 = inCircle( uv1 );
                bool   isInCircle2 = inCircle( uv2 );
                if( isInCircle0 && isInCircle1 && isInCircle2 )
                    // this is a nop since ommDataCircle is 0 initialized
                    ommDataCircle[ommIdx][uTriI / 8] |= OPTIX_OPACITY_MICROMAP_STATE_TRANSPARENT << ( ( uTriI % 8 ) * 2 );
                else if( !isInCircle0 && !isInCircle1 && !isInCircle2 && !edgeIntersectsCircle( uv0, uv1 )
                         && !edgeIntersectsCircle( uv1, uv2 ) && !edgeIntersectsCircle( uv2, uv0 ) )
                    // if all vertices are outside of the circle and no edge intersects the circle, mark it as opaque
                    // we do not need to check if the circle is fully contained by the micro triangle as the circle is already cut
                    // by the triangle that this OMM is applied to.
                    ommDataCircle[ommIdx][uTriI / 8] |= OPTIX_OPACITY_MICROMAP_STATE_OPAQUE << ( ( uTriI % 8 ) * 2 );
                else
                    // otherwise, let AH resolve it
                    ommDataCircle[ommIdx][uTriI / 8] |= OPTIX_OPACITY_MICROMAP_STATE_UNKNOWN_TRANSPARENT << ( ( uTriI % 8 ) * 2 );
            };

            // first triangle (a,b,c)
            computeOMMData( 0, tex_coords_t0 );
            // second triangle (a,c,d)
            computeOMMData( 1, tex_coords_t1 );
        }

        CUDA_CHECK( cudaMemcpy( reinterpret_cast<char*>( d_omm_input_data ) + omm_data_checkerboard_size_in_bytes, ommDataCircle.data(),
                                omm_data_circle_size_in_bytes, cudaMemcpyHostToDevice ) );
    }


    OptixOpacityMicromapArrayBuildInput bi = {};
    bi.flags                       = OPTIX_OPACITY_MICROMAP_FLAG_NONE;
    bi.inputBuffer                 = d_omm_input_data;
    bi.numMicromapHistogramEntries = (unsigned)histogram.size();
    bi.micromapHistogramEntries    = histogram.data();

    OptixMicromapBufferSizes bs = {};
    OPTIX_CHECK( optixOpacityMicromapArrayComputeMemoryUsage( state.context, &bi, &bs ) );

    // this is fairly simple, two OMMs, both with the same layout
    std::vector<OptixOpacityMicromapDesc> ommDescs =
    {
        { 
            0,
            CHECKERBOARD_OMM_SUBDIV_LEVEL, 
            OPTIX_OPACITY_MICROMAP_FORMAT_2_STATE 
        },
        { 
            static_cast<unsigned int>(ommDataCheckerboard[0].size() * sizeof(unsigned short) ), 
            CHECKERBOARD_OMM_SUBDIV_LEVEL,
            OPTIX_OPACITY_MICROMAP_FORMAT_2_STATE
        },
        { 
            omm_data_checkerboard_size_in_bytes,
            CIRCLE_OMM_SUBDIV_LEVEL,
            OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE
        },
        { 
            static_cast<unsigned int>(omm_data_checkerboard_size_in_bytes + ommDataCircle[0].size() * sizeof(unsigned short) ),
            CIRCLE_OMM_SUBDIV_LEVEL,
            OPTIX_OPACITY_MICROMAP_FORMAT_4_STATE
        }
    };

    CUdeviceptr  d_omm_desc = 0;
    const size_t omm_desc_size_in_bytes = ommDescs.size() * sizeof(OptixOpacityMicromapDesc);
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_omm_desc ), omm_desc_size_in_bytes ) );
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_omm_desc ), ommDescs.data(), omm_desc_size_in_bytes, cudaMemcpyHostToDevice ) );

    bi.perMicromapDescBuffer        = d_omm_desc;
    bi.perMicromapDescStrideInBytes = 0;

    CUdeviceptr d_temp_buffer = 0;
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_temp_buffer ), bs.tempSizeInBytes ) );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_omm_array ), bs.outputSizeInBytes ) );

    OptixMicromapBuffers uBuffers = {};
    uBuffers.output               = state.d_omm_array;
    uBuffers.outputSizeInBytes    = bs.outputSizeInBytes;
    uBuffers.temp                 = d_temp_buffer;
    uBuffers.tempSizeInBytes      = bs.tempSizeInBytes;

    OPTIX_CHECK( optixOpacityMicromapArrayBuild( state.context, 0, &bi, &uBuffers ) );

    cudaFree( reinterpret_cast<void*>( d_omm_input_data ) );
    cudaFree( reinterpret_cast<void*>( d_omm_desc ) );
    cudaFree( reinterpret_cast<void*>( d_temp_buffer ) );
}


void buildInstanceAccel( CutoutsState& state )
{
    if( state.d_ias_output_buffer )
        CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_ias_output_buffer ) ) );

    CUdeviceptr d_instances;
    size_t      instance_size_in_bytes = sizeof( OptixInstance ) * 2;
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_instances ), instance_size_in_bytes ) );

    OptixBuildInput instance_input = {};

    instance_input.type                       = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
    instance_input.instanceArray.instances    = d_instances;
    instance_input.instanceArray.numInstances = 2;

    OptixAccelBuildOptions accel_options = {};
    accel_options.buildFlags             = OPTIX_BUILD_FLAG_NONE;
    accel_options.operation              = OPTIX_BUILD_OPERATION_BUILD;

    OptixAccelBufferSizes ias_buffer_sizes;
    OPTIX_CHECK( optixAccelComputeMemoryUsage( state.context, &accel_options, &instance_input,
                                               1,  // num build inputs
                                               &ias_buffer_sizes ) );

    CUdeviceptr d_temp_buffer;
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_temp_buffer ), ias_buffer_sizes.tempSizeInBytes ) );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_ias_output_buffer ), ias_buffer_sizes.outputSizeInBytes ) );

    // Use the identity matrix for the instance transform
    Instance instance = { { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 } };

    OptixInstance optix_instances[2];
    memset( optix_instances, 0, instance_size_in_bytes );

    optix_instances[0].traversableHandle = state.triangle_gas_handle;
    optix_instances[0].flags             = ( state.enableOMMs ? OPTIX_INSTANCE_FLAG_NONE : OPTIX_INSTANCE_FLAG_DISABLE_OPACITY_MICROMAPS ) |
                                           ( state.enableAH   ? OPTIX_INSTANCE_FLAG_NONE : OPTIX_INSTANCE_FLAG_DISABLE_ANYHIT );
    optix_instances[0].instanceId        = 0;
    optix_instances[0].sbtOffset         = 0;
    optix_instances[0].visibilityMask    = 1;
    memcpy( optix_instances[0].transform, instance.transform, sizeof( float ) * 12 );

    optix_instances[1].traversableHandle = state.sphere_gas_handle;
    optix_instances[1].flags             = OPTIX_INSTANCE_FLAG_NONE;
    optix_instances[1].instanceId        = 1;
    optix_instances[1].sbtOffset         = TRIANGLE_MAT_COUNT;
    optix_instances[1].visibilityMask    = 1;
    memcpy( optix_instances[1].transform, instance.transform, sizeof( float ) * 12 );

    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_instances ), &optix_instances, instance_size_in_bytes,
                            cudaMemcpyHostToDevice ) );

    OPTIX_CHECK( optixAccelBuild( state.context,
                                  0,  // CUDA stream
                                  &accel_options,
                                  &instance_input,
                                  1,  // num build inputs
                                  d_temp_buffer,
                                  ias_buffer_sizes.tempSizeInBytes,
                                  state.d_ias_output_buffer,
                                  ias_buffer_sizes.outputSizeInBytes,
                                  &state.ias_handle,
                                  nullptr,  // emitted property list
                                  0         // num emitted properties
                                  ) );

    state.params.handle = state.ias_handle;

    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_temp_buffer ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( d_instances   ) ) );
}


void createModule( CutoutsState& state )
{
    OptixModuleCompileOptions module_compile_options = {};
#if !defined( NDEBUG )
    module_compile_options.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
    module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#endif

    state.pipeline_compile_options.usesMotionBlur        = false;
    state.pipeline_compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
    state.pipeline_compile_options.numPayloadValues      = 2;
    state.pipeline_compile_options.numAttributeValues    = whitted::NUM_ATTRIBUTE_VALUES;
    state.pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
    state.pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
    state.pipeline_compile_options.allowOpacityMicromaps            = 1;

    size_t      inputSize = 0;
    const char* input     = sutil::getInputData( OPTIX_SAMPLE_NAME, OPTIX_SAMPLE_DIR, "optixCutouts.cu", inputSize );
    OPTIX_CHECK_LOG( optixModuleCreateFromPTX(
                state.context,
                &module_compile_options,
                &state.pipeline_compile_options,
                input,
                inputSize,
                LOG, &LOG_SIZE,
                &state.module
                ) );

    input = sutil::getInputData( nullptr, nullptr, "sphere.cu", inputSize );
    OPTIX_CHECK_LOG( optixModuleCreateFromPTX(
                state.context,
                &module_compile_options,
                &state.pipeline_compile_options,
                input,
                inputSize,
                LOG, &LOG_SIZE,
                &state.sphere_module
                ) );
}


void createProgramGroups( CutoutsState& state )
{
    OptixProgramGroupOptions program_group_options = {};

    OptixProgramGroupDesc raygen_prog_group_desc    = {};
    raygen_prog_group_desc.kind                     = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    raygen_prog_group_desc.raygen.module            = state.module;
    raygen_prog_group_desc.raygen.entryFunctionName = "__raygen__rg";

    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &raygen_prog_group_desc,
                                              1,  // num program groups
                                              &program_group_options, LOG, &LOG_SIZE, &state.raygen_prog_group ) );

    OptixProgramGroupDesc miss_prog_group_desc  = {};
    miss_prog_group_desc.kind                   = OPTIX_PROGRAM_GROUP_KIND_MISS;
    miss_prog_group_desc.miss.module            = state.module;
    miss_prog_group_desc.miss.entryFunctionName = "__miss__radiance";
    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &miss_prog_group_desc,
                                              1,  // num program groups
                                              &program_group_options, LOG, &LOG_SIZE, &state.radiance_miss_group ) );

    memset( &miss_prog_group_desc, 0, sizeof( OptixProgramGroupDesc ) );
    miss_prog_group_desc.kind                   = OPTIX_PROGRAM_GROUP_KIND_MISS;
    miss_prog_group_desc.miss.module            = state.module;
    miss_prog_group_desc.miss.entryFunctionName = "__miss__occlusion";
    OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &miss_prog_group_desc,
                                              1,  // num program groups
                                              &program_group_options, LOG, &LOG_SIZE, &state.occlusion_miss_group ) );

    OptixProgramGroupDesc hit_prog_group_desc        = {};
    hit_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    hit_prog_group_desc.hitgroup.moduleCH            = state.module;
    hit_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__radiance";
    hit_prog_group_desc.hitgroup.moduleAH            = state.module;

    {
        hit_prog_group_desc.hitgroup.entryFunctionNameAH = "__anyhit__ah_checkerboard";
        OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &hit_prog_group_desc,
                                                  1,  // num program groups
                                                  &program_group_options, LOG, &LOG_SIZE, &state.triangle_checkerboard_hit_group ) );
    }
    {
        hit_prog_group_desc.hitgroup.entryFunctionNameAH = "__anyhit__ah_circle";
        OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &hit_prog_group_desc,
                                                  1,  // num program groups
                                                  &program_group_options, LOG, &LOG_SIZE, &state.triangle_circle_hit_group ) );
    }
    {
        hit_prog_group_desc.hitgroup.entryFunctionNameAH = "__anyhit__ah_checkerboard";
        hit_prog_group_desc.hitgroup.moduleIS            = state.sphere_module;
        hit_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__sphere";
        OPTIX_CHECK_LOG( optixProgramGroupCreate( state.context, &hit_prog_group_desc,
                                                  1,  // num program groups
                                                  &program_group_options, LOG, &LOG_SIZE, &state.sphere_checkerboard_hit_group ) );
    }

}


void createPipeline( CutoutsState& state )
{
    const uint32_t    max_trace_depth = 2;
    OptixProgramGroup program_groups[] =
    {
        state.raygen_prog_group,
        state.radiance_miss_group,
        state.occlusion_miss_group,
        state.triangle_checkerboard_hit_group,
        state.triangle_circle_hit_group,
        state.sphere_checkerboard_hit_group
    };

    OptixPipelineLinkOptions pipeline_link_options = {};
    pipeline_link_options.maxTraceDepth            = max_trace_depth;
    pipeline_link_options.debugLevel               = OPTIX_COMPILE_DEBUG_LEVEL_FULL;

    OPTIX_CHECK_LOG( optixPipelineCreate( state.context,
                                          &state.pipeline_compile_options,
                                          &pipeline_link_options,
                                          program_groups,
                                          sizeof( program_groups ) / sizeof( program_groups[0] ),
                                          LOG, &LOG_SIZE,
                                          &state.pipeline ) );

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
    OPTIX_CHECK( optixPipelineSetStackSize( state.pipeline, direct_callable_stack_size_from_traversal,
                                            direct_callable_stack_size_from_state, continuation_stack_size,
                                            2  // maxTraversableDepth
                                            ) );
}


void createSBT( CutoutsState& state )
{
    // texture coordinates are custom application state, not part of any primitive data!
    const size_t tex_coords_size_in_bytes = g_tex_coords.size() * sizeof( float2 );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &state.d_tex_coords ), tex_coords_size_in_bytes ) );
    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( state.d_tex_coords ), g_tex_coords.data(),
                            tex_coords_size_in_bytes, cudaMemcpyHostToDevice ) );

    CUdeviceptr  d_raygen_record;
    const size_t raygen_record_size = sizeof( RayGenRecord );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_raygen_record ), raygen_record_size ) );

    RayGenRecord rg_sbt;
    OPTIX_CHECK( optixSbtRecordPackHeader( state.raygen_prog_group, &rg_sbt ) );
    rg_sbt.data = {1.0f, 0.f, 0.f};

    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_raygen_record ), &rg_sbt, raygen_record_size,
                            cudaMemcpyHostToDevice ) );

    // two miss programs:
    // first for 'radiance' rays, return the 'background' color
    // second for 'occlusion' rays, marking a ray as 'unoccluded' if miss is executed.
    CUdeviceptr  d_miss_records;
    const size_t miss_record_size = sizeof( MissRecord );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_miss_records ), miss_record_size * 2 ) );

    MissRecord ms_sbt[2];
    OPTIX_CHECK( optixSbtRecordPackHeader( state.radiance_miss_group, &ms_sbt[0] ) );
    ms_sbt[0].data = {0.0f, 0.0f, 0.0f};
    OPTIX_CHECK( optixSbtRecordPackHeader( state.occlusion_miss_group, &ms_sbt[1] ) );
    ms_sbt[1].data = {}; // no data needed for occlusion miss program

    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_miss_records ), ms_sbt, miss_record_size * 2,
                            cudaMemcpyHostToDevice ) );

    CUdeviceptr  d_hitgroup_records;
    const size_t hitgroup_record_size = sizeof( HitGroupRecord );
    CUDA_CHECK( cudaMalloc( reinterpret_cast<void**>( &d_hitgroup_records ),
                            hitgroup_record_size * ( TRIANGLE_MAT_COUNT + SPHERE_MAT_COUNT ) ) );

    HitGroupRecord hitgroup_records[TRIANGLE_MAT_COUNT + SPHERE_MAT_COUNT];

    // Set up the HitGroupRecords for the triangle materials
    for( int sbt_idx = 0; sbt_idx < TRIANGLE_MAT_COUNT; ++sbt_idx )
    {
        // Apply hitgroup with checkerboard cutout in AH as default.
        // The last triangle material uses a different hitgroup for the circular cutout,
        // this material / sbt is applied to two faces of the small box.
        // Both hit groups only differ in the AH though, which is disabled for all triangles referencing materials [0,3].
        // These triangles use OMM predefined index 'opaque'.
        OPTIX_CHECK( optixSbtRecordPackHeader( sbt_idx != TRIANGLE_MAT_COUNT - 1 ? state.triangle_checkerboard_hit_group :
                                                                                   state.triangle_circle_hit_group,
                                               &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.emission_color = g_emission_colors[sbt_idx];
        hitgroup_records[sbt_idx].data.diffuse_color  = g_diffuse_colors[sbt_idx];
        hitgroup_records[sbt_idx].data.vertices       = reinterpret_cast<float4*>( state.d_vertices );
        hitgroup_records[sbt_idx].data.tex_coords     = reinterpret_cast<float2*>( state.d_tex_coords );
    }

    // Set up the HitGroupRecords for the sphere material
    for( int sbt_idx = TRIANGLE_MAT_COUNT; sbt_idx < TRIANGLE_MAT_COUNT + SPHERE_MAT_COUNT; ++sbt_idx )
    {
        OPTIX_CHECK( optixSbtRecordPackHeader( state.sphere_checkerboard_hit_group, &hitgroup_records[sbt_idx] ) );
        hitgroup_records[sbt_idx].data.emission_color = g_sphere_emission_color;
        hitgroup_records[sbt_idx].data.diffuse_color  = g_sphere_diffuse_color;
        hitgroup_records[sbt_idx].data.geometry_data.setSphere( g_sphere );
    }

    CUDA_CHECK( cudaMemcpy( reinterpret_cast<void*>( d_hitgroup_records ), hitgroup_records,
                            hitgroup_record_size * ( TRIANGLE_MAT_COUNT + SPHERE_MAT_COUNT ),
                            cudaMemcpyHostToDevice ) );

    state.sbt.raygenRecord                = d_raygen_record;
    state.sbt.missRecordBase              = d_miss_records;
    state.sbt.missRecordStrideInBytes     = static_cast<uint32_t>( miss_record_size );
    state.sbt.missRecordCount             = 2; // one for 'radiance' rays and one for 'occlusion' rays
    state.sbt.hitgroupRecordBase          = d_hitgroup_records;
    state.sbt.hitgroupRecordStrideInBytes = static_cast<uint32_t>( hitgroup_record_size );
    state.sbt.hitgroupRecordCount         = TRIANGLE_MAT_COUNT + SPHERE_MAT_COUNT;
}


void cleanupState( CutoutsState& state )
{
    OPTIX_CHECK( optixPipelineDestroy( state.pipeline ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.raygen_prog_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.radiance_miss_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.triangle_checkerboard_hit_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.triangle_circle_hit_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.sphere_checkerboard_hit_group ) );
    OPTIX_CHECK( optixProgramGroupDestroy( state.occlusion_miss_group ) );
    OPTIX_CHECK( optixModuleDestroy( state.module ) );
    OPTIX_CHECK( optixModuleDestroy( state.sphere_module ) );
    OPTIX_CHECK( optixDeviceContextDestroy( state.context ) );

    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.sbt.raygenRecord ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.sbt.missRecordBase ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.sbt.hitgroupRecordBase ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_vertices ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_tex_coords ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_omm_array ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_triangle_gas_output_buffer ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_sphere_gas_output_buffer ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.params.accum_buffer ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_params ) ) );
    CUDA_CHECK( cudaFree( reinterpret_cast<void*>( state.d_ias_output_buffer ) ) );
}


//------------------------------------------------------------------------------
//
// Main
//
//------------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    CutoutsState state; // init with values as specified at definition
    state.params.width  = 768;
    state.params.height = 768;
    sutil::CUDAOutputBufferType output_buffer_type = sutil::CUDAOutputBufferType::GL_INTEROP;

    //
    // Parse command line options
    //
    std::string outfile;

    for( int i = 1; i < argc; ++i )
    {
        const std::string arg = argv[i];
        if( arg == "--help" || arg == "-h" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "--no-gl-interop" )
        {
            output_buffer_type = sutil::CUDAOutputBufferType::CUDA_DEVICE;
            use_pbo = false;
        }
        else if( arg == "--file" || arg == "-f" )
        {
            if( i >= argc - 1 )
                printUsageAndExit( argv[0] );
            outfile = argv[++i];
        }
        else if( arg.substr( 0, 6 ) == "--dim=" )
        {
            const std::string dims_arg = arg.substr( 6 );
            int               w, h;
            sutil::parseDimensions( dims_arg.c_str(), w, h );
            state.params.width  = w;
            state.params.height = h;
        }
        else if( arg == "--launch-samples" || arg == "-s" )
        {
            if( i >= argc - 1 )
                printUsageAndExit( argv[0] );
            samples_per_launch = atoi( argv[++i] );
        }
        else
        {
            std::cerr << "Unknown option '" << argv[i] << "'\n";
            printUsageAndExit( argv[0] );
        }
    }

    try
    {
        initCameraState();

        //
        // Set up OptiX state
        //
        createContext                   ( state );
        buildCheckerboardOpacityMicromap( state );
        buildGeomAccel                  ( state );
        buildInstanceAccel              ( state );
        createModule                    ( state );
        createProgramGroups             ( state );
        createPipeline                  ( state );
        createSBT                       ( state );
        initLaunchParams                ( state );


        if( outfile.empty() )
        {
            std::cout << "////////////////////////////////////////////////////////////////////////////////////////////////////////////\n";
            std::cout << "Keys:\n";
            std::cout << "         Q    Quit sample\n";
            std::cout << "         A    Toggle usage of anyhit program (AH) on small block (checkerboard and circle cutout)\n";
            std::cout << "         O    Toggle usage of opacity micromaps (OMM) on small block (checkerboard and circle cutout)\n";
            std::cout << "\n";
            std::cout << "Default: OMMs and AH are enabled.\n";
            std::cout << "Toggle behavior:\n";
            std::cout << "Having OMMs disabled, but AH enabled will cause all hits of the small block to be resolve by AH. This has no visual difference to OMMs and AH being enabled.\n";
            std::cout << "Having OMMs enabled, but AH disabled will turn all micro triangles with opacity states 'opaque' and 'unknown opaque' to opaque, causing a 'jaggy' circular cutout.\n";
            std::cout << "Having both, OMMs and AH disabled will cause all intersections on the small block to be accepted, i.e., fully opaque triangles.\n";
            std::cout << "////////////////////////////////////////////////////////////////////////////////////////////////////////////\n";

            GLFWwindow* window = sutil::initUI( "optixCutouts", state.params.width, state.params.height );
            glfwSetMouseButtonCallback  ( window, mouseButtonCallback   );
            glfwSetCursorPosCallback    ( window, cursorPosCallback     );
            glfwSetWindowSizeCallback   ( window, windowSizeCallback    );
            glfwSetWindowIconifyCallback( window, windowIconifyCallback );
            glfwSetKeyCallback          ( window, keyCallback           );
            glfwSetScrollCallback       ( window, scrollCallback        );
            glfwSetWindowUserPointer    ( window, &state                );

            //
            // Render loop
            //
            {
                sutil::CUDAOutputBuffer<uchar4> output_buffer( output_buffer_type, state.params.width, state.params.height );
                output_buffer.setStream( state.stream );
                sutil::GLDisplay gl_display;

                std::chrono::duration<double> state_update_time( 0.0 );
                std::chrono::duration<double> render_time( 0.0 );
                std::chrono::duration<double> display_time( 0.0 );

                do
                {
                    auto t0 = std::chrono::steady_clock::now();
                    glfwPollEvents();

                    updateState( output_buffer, state.params );
                    auto t1 = std::chrono::steady_clock::now();
                    state_update_time += t1 - t0;
                    t0 = t1;

                    launchSubframe( output_buffer, state );
                    t1 = std::chrono::steady_clock::now();
                    render_time += t1 - t0;
                    t0 = t1;

                    displaySubframe( output_buffer, gl_display, window );
                    t1 = std::chrono::steady_clock::now();
                    display_time += t1 - t0;

                    sutil::displayStats( state_update_time, render_time, display_time );

                    glfwSwapBuffers(window);

                    ++state.params.subframe_index;
                }
                while( !glfwWindowShouldClose( window ) );
                CUDA_SYNC_CHECK();
            }

            sutil::cleanupUI( window );
        }
        else
        {
            if( use_pbo)
            {
                sutil::initGLFW(); // For GL context
                sutil::initGL();
            }

            sutil::CUDAOutputBuffer<uchar4> output_buffer(output_buffer_type, state.params.width, state.params.height);
            handleCameraUpdate(state.params);
            handleResize(output_buffer, state.params);
            launchSubframe(output_buffer, state);

            sutil::ImageBuffer buffer;
            buffer.data = output_buffer.getHostPointer();
            buffer.width = output_buffer.width();
            buffer.height = output_buffer.height();
            buffer.pixel_format = sutil::BufferImageFormat::UNSIGNED_BYTE4;

            sutil::saveImage(outfile.c_str(), buffer, false);

            glfwTerminate();
        }

        cleanupState( state );

    }
    catch( std::exception& e )
    {
        std::cerr << "Caught exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
