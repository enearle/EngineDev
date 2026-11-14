#include "Common.hlsli"

struct VSInput
{
    float3 position : POSITION;
    float4 boneWeights : BLENDWEIGHT;
    uint4 boneIndices : BLENDINDICES;
};

struct VSOutput
{
    float4 position : SV_Position;
    float depth : TEXCOORD0;
};

VSOutput main(VSInput input, uint instanceID : SV_InstanceID, uint viewID : SV_ViewID)
{
    InstanceData instance = LoadInstance(instanceID);
    float4x4 skin = SkinMatrix(instance.BoneOffset, input.boneWeights, input.boneIndices);
    float4 worldPosition = mul(instance.World, mul(skin, float4(input.position, 1.0)));

    VSOutput output;
    output.position = mul(LightViewProjection[viewID], worldPosition);
    output.depth = output.position.w / Lights[viewID].Radius;
    return output;
}
