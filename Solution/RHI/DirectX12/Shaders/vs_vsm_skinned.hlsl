#define MAX_BONES 128

struct RootConstants
{
    float4x4 model;
    float4x4 normal;
};
ConstantBuffer<RootConstants> ModelData : register(b0, space0);

struct CBVBuffer
{
    float4x4 viewProjection [4];
};
ConstantBuffer<CBVBuffer> LightMatrices : register(b0, space1);

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

cbuffer LightData : register(b0, space2)
{
    Light Lights[4];
};

cbuffer BoneData : register(b0, space16)
{
    float4x4 bones[MAX_BONES];
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
    float4 boneWeights : BLENDWEIGHT;
    uint4 boneIndices : BLENDINDICES;
};

struct VSOutput {
    float4 position : SV_Position;
    float depth : TEXCOORD0;
};

VSOutput main(VSInput input, uint viewID : SV_ViewID) {
    VSOutput output;
    
    float4 skinnedPos = float4(0, 0, 0, 0);
    for (int i = 0; i < 4; i++)
    {
        float weight = input.boneWeights[i];
        if (weight > 0.0)
        {
            uint boneIndex = input.boneIndices[i];
            float4x4 boneTransform = bones[boneIndex];
            
            skinnedPos += weight * mul(boneTransform, float4(input.position, 1.0));
        }
    }
    
    float4 worldPosition = mul(ModelData.model, skinnedPos);
    float4 clipPos = mul(LightMatrices.viewProjection[viewID], worldPosition);
    output.position = clipPos;

    float linearZ = clipPos.w;
    output.depth = linearZ / Lights[viewID].Radius;
    return output;
}