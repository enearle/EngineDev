Texture2D albedoMap : register(t0, space2);
Texture2D normalMap : register(t1, space2);
Texture2D metallicRoughnessMap : register(t2, space2);
SamplerState linearSampler : register(s0, space0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
};

struct PSOutput
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 metallicRoughness : SV_TARGET2;
    float4 position : SV_TARGET3;
};

PSOutput main(PSInput input)
{
    PSOutput output;
    float3 tangentNormal = normalMap.Sample(linearSampler, input.uv).rgb * 2.0 - 1.0;
    float3x3 TBN = float3x3(
       normalize(input.tangent), 
       -normalize(input.binormal), 
       normalize(input.normal)
   );
    
    float3 worldNormal = normalize(mul(tangentNormal, TBN));
    output.albedo = albedoMap.Sample(linearSampler, input.uv);
    output.normal = float4(worldNormal, 1.0f);
    output.metallicRoughness = metallicRoughnessMap.Sample(linearSampler, input.uv);
    output.position = float4(input.worldPosition, 1.0);
    
    return output;
}