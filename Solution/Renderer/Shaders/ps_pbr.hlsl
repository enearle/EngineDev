#include "Common.hlsli"

Texture2D albedoMap : register(t0, space2);
Texture2D normalMap : register(t1, space2);
Texture2D metallicRoughnessMap : register(t2, space2);
SamplerState linearSampler : register(s0, space0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 albedo : SV_TARGET0;
    float2 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

PSOutput main(PSInput input)
{
    float3 tangentNormal = normalMap.Sample(linearSampler, input.uv).rgb * 2.0 - 1.0;
    float3x3 tangentFrame = float3x3(normalize(input.tangent), -normalize(input.binormal), normalize(input.normal));

    PSOutput output;
    output.albedo = albedoMap.Sample(linearSampler, input.uv);
    output.normal = EncodeNormal(normalize(mul(tangentNormal, tangentFrame)));
    output.material = metallicRoughnessMap.Sample(linearSampler, input.uv);
    return output;
}
