#include "Common.hlsli"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
    float3 worldPosition : TEXCOORD1;
};

VSOutput main(VSInput input, uint instanceID : SV_InstanceID)
{
    InstanceData instance = LoadInstance(instanceID);
    VSOutput output;

    float4 worldPosition = mul(instance.World, float4(input.position, 1.0));
    output.position = mul(ViewProjection, worldPosition);

    float3x3 normalMatrix = (float3x3)instance.Normal;
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.tangent = normalize(mul(normalMatrix, input.tangent));
    output.binormal = normalize(mul(normalMatrix, input.binormal));
    output.uv = input.uv;
    output.worldPosition = worldPosition.xyz;
    return output;
}
