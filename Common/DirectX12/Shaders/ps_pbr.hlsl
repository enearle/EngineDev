struct VSOutput
{
    float4 position         : SV_Position;
    float3 worldPosition    : TEXCOORD0;
    float3 normal           : TEXCOORD1;
    float3 tangent          : TEXCOORD2;
    float3 bitangent        : TEXCOORD3;
    float2 uv               : TEXCOORD4;
};

struct PSOutput
{
    float4 albedo   : SV_Target0;
    float4 normal   : SV_Target1;
    float4 MRA      : SV_Target2;
    float4 position : SV_Target3;
};

Texture2D albedoMap                 : register(t0);
Texture2D normalMap                 : register(t1);
Texture2D metallicRoughnessAOMap    : register(t2);
SamplerState linearSampler          : register(s0);
SamplerState pointSampler           : register(s1);

PSOutput main(VSOutput input)
{
    PSOutput output;

    float3 albedo = albedoMap.Sample(linearSampler, input.uv).rgb;
    float3 tangentNormal = normalMap.Sample(linearSampler, input.uv).rgb * 2.0 - 1.0;
    float3 mra = metallicRoughnessAOMap.Sample(linearSampler, input.uv).rgb;
    
    float3x3 TBN = float3x3(
        normalize(input.tangent), 
        -normalize(input.bitangent), 
        normalize(input.normal)
    );
    
    float3 worldNormal = normalize(mul(tangentNormal, TBN));

    output.albedo   = float4(albedo, 1.0f);
    output.normal   = float4(worldNormal, 1.0f);
    output.MRA      = float4(mra, 1.0f);
    output.position = float4(input.worldPosition, 1);

    return output;
}