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

#include "optixPathTracer.h"
#include "random.h"

#include <sutil/vec_math.h>
#include <cuda/helpers.h>

extern "C" {
__constant__ Params params;
}



//------------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------------


struct Onb
{
  __forceinline__ __device__ Onb(const float3& normal)
  {
    m_normal = normal;

    if( fabs(m_normal.x) > fabs(m_normal.z) )
    {
      m_binormal.x = -m_normal.y;
      m_binormal.y =  m_normal.x;
      m_binormal.z =  0;
    }
    else
    {
      m_binormal.x =  0;
      m_binormal.y = -m_normal.z;
      m_binormal.z =  m_normal.y;
    }

    m_binormal = normalize(m_binormal);
    m_tangent = cross( m_binormal, m_normal );
  }

  __forceinline__ __device__ void inverse_transform(float3& p) const
  {
    p = p.x*m_tangent + p.y*m_binormal + p.z*m_normal;
  }

  float3 m_tangent;
  float3 m_binormal;
  float3 m_normal;
};


//------------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------------


static __forceinline__ __device__ RadiancePRD loadClosesthitRadiancePRD()
{
    RadiancePRD prd = {};

    prd.attenuation.x = __uint_as_float( optixGetPayload_0() );
    prd.attenuation.y = __uint_as_float( optixGetPayload_1() );
    prd.attenuation.z = __uint_as_float( optixGetPayload_2() );
    prd.seed  = optixGetPayload_3();
    prd.depth = optixGetPayload_4();
    return prd;
}

static __forceinline__ __device__ RadiancePRD loadMissRadiancePRD()
{
    RadiancePRD prd = {};
    return prd;
}

static __forceinline__ __device__ void storeClosesthitRadiancePRD( RadiancePRD prd )
{
    optixSetPayload_0( __float_as_uint( prd.attenuation.x ) );
    optixSetPayload_1( __float_as_uint( prd.attenuation.y ) );
    optixSetPayload_2( __float_as_uint( prd.attenuation.z ) );

    optixSetPayload_3( prd.seed );
    optixSetPayload_4( prd.depth );

    optixSetPayload_5( __float_as_uint( prd.emitted.x ) );
    optixSetPayload_6( __float_as_uint( prd.emitted.y ) );
    optixSetPayload_7( __float_as_uint( prd.emitted.z ) );

    optixSetPayload_8( __float_as_uint( prd.radiance.x ) );
    optixSetPayload_9( __float_as_uint( prd.radiance.y ) );
    optixSetPayload_10( __float_as_uint( prd.radiance.z ) );

    optixSetPayload_11( __float_as_uint( prd.origin.x ) );
    optixSetPayload_12( __float_as_uint( prd.origin.y ) );
    optixSetPayload_13( __float_as_uint( prd.origin.z ) );

    optixSetPayload_14( __float_as_uint( prd.direction.x ) );
    optixSetPayload_15( __float_as_uint( prd.direction.y ) );
    optixSetPayload_16( __float_as_uint( prd.direction.z ) );

    optixSetPayload_17( prd.done );
}


static __forceinline__ __device__ void storeMissRadiancePRD( RadiancePRD prd )
{
    optixSetPayload_5( __float_as_uint( prd.emitted.x ) );
    optixSetPayload_6( __float_as_uint( prd.emitted.y ) );
    optixSetPayload_7( __float_as_uint( prd.emitted.z ) );

    optixSetPayload_8( __float_as_uint( prd.radiance.x ) );
    optixSetPayload_9( __float_as_uint( prd.radiance.y ) );
    optixSetPayload_10( __float_as_uint( prd.radiance.z ) );

    optixSetPayload_17( prd.done );
}


static __forceinline__ __device__ void storeOcclusionPRD( bool occluded )
{
    optixSetPayload_0( static_cast<unsigned int>( occluded ) );
}


static __forceinline__ __device__ void cosine_sample_hemisphere(const float u1, const float u2, float3& p)
{
  // Uniformly sample disk.
  const float r   = sqrtf( u1 );
  const float phi = 2.0f*M_PIf * u2;
  p.x = r * cosf( phi );
  p.y = r * sinf( phi );

  // Project up to hemisphere.
  p.z = sqrtf( fmaxf( 0.0f, 1.0f - p.x*p.x - p.y*p.y ) );
}


static __forceinline__ __device__ void traceRadiance(
        OptixTraversableHandle handle,
        float3                 ray_origin,
        float3                 ray_direction,
        float                  tmin,
        float                  tmax,
        RadiancePRD&           prd
        )
{
    unsigned int u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, u10, u11, u12, u13, u14, u15, u16, u17;

    u0 = __float_as_uint( prd.attenuation.x );
    u1 = __float_as_uint( prd.attenuation.y );
    u2 = __float_as_uint( prd.attenuation.z );
    u3 = prd.seed;
    u4 = prd.depth;

    optixTrace(
            PAYLOAD_TYPE_RADIANCE,
            handle,
            ray_origin,
            ray_direction,
            tmin,
            tmax,
            0.0f,                // rayTime
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_NONE,
            RAY_TYPE_RADIANCE,        // SBT offset
            RAY_TYPE_COUNT,           // SBT stride
            RAY_TYPE_RADIANCE,        // missSBTIndex
            u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, u10, u11, u12, u13, u14, u15, u16, u17 );

    prd.attenuation = make_float3( __uint_as_float( u0 ), __uint_as_float( u1 ), __uint_as_float( u2 ) );
    prd.seed  = u3;
    prd.depth = u4;

    prd.emitted   = make_float3( __uint_as_float( u5 ), __uint_as_float( u6 ), __uint_as_float( u7 ) );
    prd.radiance  = make_float3( __uint_as_float( u8 ), __uint_as_float( u9 ), __uint_as_float( u10 ) );
    prd.origin    = make_float3( __uint_as_float( u11 ), __uint_as_float( u12 ), __uint_as_float( u13 ) );
    prd.direction = make_float3( __uint_as_float( u14 ), __uint_as_float( u15 ), __uint_as_float( u16 ) );
    prd.done = u17;
}


static __forceinline__ __device__ bool traceOcclusion(
        OptixTraversableHandle handle,
        float3                 ray_origin,
        float3                 ray_direction,
        float                  tmin,
        float                  tmax
        )
{
    unsigned int occluded = 0u;
    optixTrace(
            PAYLOAD_TYPE_OCCLUSION,
            handle,
            ray_origin,
            ray_direction,
            tmin,
            tmax,
            0.0f,                    // rayTime
            OptixVisibilityMask( 1 ),
            OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT,
            RAY_TYPE_OCCLUSION,      // SBT offset
            RAY_TYPE_COUNT,          // SBT stride
            RAY_TYPE_OCCLUSION,      // missSBTIndex
            occluded );
    return occluded;
}


//------------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------------

extern "C" __global__ void __raygen__rg()
{
    const int    w   = params.width;
    const int    h   = params.height;
    const float3 eye = params.eye;
    const float3 U   = params.U;
    const float3 V   = params.V;
    const float3 W   = params.W;
    const uint3  idx = optixGetLaunchIndex();
    const int    subframe_index = params.subframe_index;

    unsigned int seed = tea<4>( idx.y*w + idx.x, subframe_index );

    float3 result = make_float3( 0.0f );
    int i = params.samples_per_launch;
    do
    {
        // The center of each pixel is at fraction (0.5,0.5)
        const float2 subpixel_jitter = make_float2( rnd( seed ), rnd( seed ) );

        const float2 d = 2.0f * make_float2(
                ( static_cast<float>( idx.x ) + subpixel_jitter.x ) / static_cast<float>( w ),
                ( static_cast<float>( idx.y ) + subpixel_jitter.y ) / static_cast<float>( h )
                ) - 1.0f;
        float3 ray_direction = normalize(d.x*U + d.y*V + W);
        float3 ray_origin    = eye;

        RadiancePRD prd;
        prd.attenuation  = make_float3(1.f);
        prd.seed         = seed;
        prd.depth        = 0;        

        for( ;; )
        {
            traceRadiance(
                    params.handle,
                    ray_origin,
                    ray_direction,
                    0.01f,  // tmin       // TODO: smarter offset
                    1e16f,  // tmax
                    prd );

            result += prd.emitted;
            result += prd.radiance * prd.attenuation;

            if( prd.done  || prd.depth >= 3 ) // TODO RR, variable for depth
                break;

            ray_origin    = prd.origin;
            ray_direction = prd.direction;

            ++prd.depth;
        }
    }
    while( --i );

    const uint3    launch_index = optixGetLaunchIndex();
    const unsigned int image_index  = launch_index.y * params.width + launch_index.x;
    float3         accum_color  = result / static_cast<float>( params.samples_per_launch );

    if( subframe_index > 0 )
    {
        const float                 a = 1.0f / static_cast<float>( subframe_index+1 );
        const float3 accum_color_prev = make_float3( params.accum_buffer[ image_index ]);
        accum_color = lerp( accum_color_prev, accum_color, a );
    }
    params.accum_buffer[ image_index ] = make_float4( accum_color, 1.0f);
    params.frame_buffer[ image_index ] = make_color ( accum_color );
}


extern "C" __global__ void __miss__occlusion()
{
    optixSetPayloadTypes( PAYLOAD_TYPE_OCCLUSION );

    storeOcclusionPRD( false );
}


extern "C" __global__ void __miss__radiance()
{
    optixSetPayloadTypes( PAYLOAD_TYPE_RADIANCE );

    MissData* rt_data  = reinterpret_cast<MissData*>( optixGetSbtDataPointer() );
    RadiancePRD prd = loadMissRadiancePRD();

    prd.radiance  = make_float3( rt_data->bg_color );
    prd.emitted   = make_float3( 0.f );
    prd.done      = true;

    storeMissRadiancePRD( prd );
}


extern "C" __global__ void __closesthit__occlusion()
{
    optixSetPayloadTypes( PAYLOAD_TYPE_OCCLUSION );

    storeOcclusionPRD( true );
}


extern "C" __global__ void __closesthit__radiance()
{
    optixSetPayloadTypes( PAYLOAD_TYPE_RADIANCE );

    HitGroupData* rt_data = (HitGroupData*)optixGetSbtDataPointer();

    const int    prim_idx        = optixGetPrimitiveIndex();
    const float3 ray_dir         = optixGetWorldRayDirection();
    const int    vert_idx_offset = prim_idx*3;

    const float3 v0   = make_float3( rt_data->vertices[ vert_idx_offset+0 ] );
    const float3 v1   = make_float3( rt_data->vertices[ vert_idx_offset+1 ] );
    const float3 v2   = make_float3( rt_data->vertices[ vert_idx_offset+2 ] );
    const float3 N_0  = normalize( cross( v1-v0, v2-v0 ) );

    const float3 N    = faceforward( N_0, -ray_dir, N_0 );
    const float3 P    = optixGetWorldRayOrigin() + optixGetRayTmax()*ray_dir;

    RadiancePRD prd = loadClosesthitRadiancePRD();

    if( prd.depth == 0 )
        prd.emitted = rt_data->emission_color;
    else
        prd.emitted = make_float3( 0.0f );


    unsigned int seed = prd.seed;

    {
        const float z1 = rnd(seed);
        const float z2 = rnd(seed);

        float3 w_in;
        cosine_sample_hemisphere( z1, z2, w_in );
        Onb onb( N );
        onb.inverse_transform( w_in );
        prd.direction = w_in;
        prd.origin    = P;

        prd.attenuation *= rt_data->diffuse_color;
    }

    const float z1 = rnd(seed);
    const float z2 = rnd(seed);
    prd.seed = seed;

    ParallelogramLight light = params.light;
    const float3 light_pos = light.corner + light.v1 * z1 + light.v2 * z2;

    // Calculate properties of light sample (for area based pdf)
    const float  Ldist = length(light_pos - P );
    const float3 L     = normalize(light_pos - P );
    const float  nDl   = dot( N, L );
    const float  LnDl  = -dot( light.normal, L );

    float weight = 0.0f;
    if( nDl > 0.0f && LnDl > 0.0f )
    {
        const bool occluded = traceOcclusion(
            params.handle,
            P,
            L,
            0.01f,         // tmin
            Ldist - 0.01f  // tmax
            );

        if( !occluded )
        {
            const float A = length(cross(light.v1, light.v2));
            weight = nDl * LnDl * A / (M_PIf * Ldist * Ldist);
        }
    }

    prd.radiance = light.emission * weight;
    prd.done     = false;

    storeClosesthitRadiancePRD( prd );
}
