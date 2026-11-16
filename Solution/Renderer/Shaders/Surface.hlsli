// The interface between ps_gbuffer and a material type's surface file, which defines
// EvaluateSurface. A type's parameter block is a cbuffer at b0 and its textures are t1 to t8, all in
// space2; the fields and slots match the type's registration in C++.
#ifndef SURFACE_HLSLI
#define SURFACE_HLSLI

#include "Colour.hlsli"
#include "Common.hlsli"

struct SurfaceInput
{
    float3 WorldPosition;
    float3 Normal;
    float3 Tangent;
    float3 Bitangent;
    float2 UV;
    float4 PixelPosition;
};

// Albedo is linear colour and Normal a unit world-space normal.
struct Surface
{
    float3 Albedo;
    float3 Normal;
    float Metallic;
    float Roughness;
    float Occlusion;
};

SamplerState MaterialSampler : register(s0, space0);
SamplerState PointClampSampler : register(s1, space0);
SamplerState LinearClampSampler : register(s2, space0);

#endif
