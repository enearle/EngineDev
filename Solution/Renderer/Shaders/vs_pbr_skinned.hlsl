#include "Common.hlsli"

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

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData instance = LoadInstance(instanceID);
    float4x4 skin = SkinMatrix(instance.BoneOffset, input.boneWeights, input.boneIndices);
    VSOutput output;

    float4 worldPosition = mul(instance.World, mul(skin, float4(input.position, 1.0)));
    output.position = mul(ViewProjection, worldPosition);

    float3x3 normalMatrix = mul((float3x3)instance.Normal, (float3x3)skin);
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.tangent = normalize(mul(normalMatrix, input.tangent));
    output.binormal = normalize(mul(normalMatrix, input.binormal));
    output.uv = input.uv;
    return output;
}
