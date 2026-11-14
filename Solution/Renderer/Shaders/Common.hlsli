// Mirrors FrameConstants, InstanceData and DrawConstants in Renderer/RenderScene.h.
#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define MAX_LIGHTS 4

struct Light
{
    float3 Position;
    float3 Colour;
    float Intensity;
    float Radius;
    uint Type;
    float Angle;
    uint ShadowIndex;
};

cbuffer FrameConstants : register(b0, space1)
{
    float4x4 ViewProjection;
    float4x4 InverseViewProjection;
    float4 CameraPosition;
    float4x4 LightViewProjection[MAX_LIGHTS];
    Light Lights[MAX_LIGHTS];
};

struct InstanceData
{
    float4x4 World;
    float4x4 Normal;
    uint BoneOffset;
    uint Padding0;
    uint Padding1;
    uint Padding2;
};

struct BoneData
{
    float4x4 Transform;
};

StructuredBuffer<InstanceData> Instances : register(t1, space1);
StructuredBuffer<BoneData> Bones : register(t2, space1);

struct DrawConstantsData
{
    uint FirstInstance;
    uint3 Padding;
};

[[vk::push_constant]]
ConstantBuffer<DrawConstantsData> Draw : register(b0, space999);

InstanceData LoadInstance(uint instanceID)
{
    return Instances[Draw.FirstInstance + instanceID];
}

float4x4 SkinMatrix(uint boneOffset, float4 weights, uint4 indices)
{
    float4x4 skin = (float4x4)0;
    for (int i = 0; i < 4; i++)
        if (weights[i] > 0.0)
            skin += weights[i] * Bones[boneOffset + indices[i]].Transform;
    return skin;
}

float2 OctahedralWrap(float2 v)
{
    float2 signs = float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
    return (1.0 - abs(v.yx)) * signs;
}

// Unit normal to [0, 1]^2 by octahedral mapping.
float2 EncodeNormal(float3 n)
{
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    float2 encoded = n.z >= 0.0 ? n.xy : OctahedralWrap(n.xy);
    return encoded * 0.5 + 0.5;
}

float3 DecodeNormal(float2 encoded)
{
    float2 f = encoded * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

#endif
