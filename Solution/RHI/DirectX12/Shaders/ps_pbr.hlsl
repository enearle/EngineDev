Texture2D<float4> albedoMap : register(t0, space1);
SamplerState _albedoMap_sampler : register(s0, space1);
Texture2D<float4> normalMap : register(t1, space1);
SamplerState _normalMap_sampler : register(s1, space1);
Texture2D<float4> metallicRoughnessMap : register(t2, space1);
SamplerState _metallicRoughnessMap_sampler : register(s2, space1);

static float2 inUV;
static float3 inTangent;
static float3 inBitangent;
static float3 inNormal;
static float4 outAlbedo;
static float4 outNormal;
static float4 outMRA;
static float4 outPosition;
static float3 inWorldPosition;

struct SPIRV_Cross_Input
{
    float3 inWorldPosition : TEXCOORD0;
    float3 inNormal : TEXCOORD1;
    float3 inTangent : TEXCOORD2;
    float3 inBitangent : TEXCOORD3;
    float2 inUV : TEXCOORD4;
};

struct SPIRV_Cross_Output
{
    float4 outAlbedo : SV_Target0;
    float4 outNormal : SV_Target1;
    float4 outMRA : SV_Target2;
    float4 outPosition : SV_Target3;
};

void frag_main()
{
    outAlbedo = float4(albedoMap.Sample(_albedoMap_sampler, inUV).xyz, 1.0f);
    outNormal = float4(normalize(mul((normalMap.Sample(_normalMap_sampler, inUV).xyz * 2.0f) - 1.0f.xxx, float3x3(float3(normalize(inTangent)), float3(-normalize(inBitangent)), float3(normalize(inNormal))))), 1.0f);
    outMRA = float4(metallicRoughnessMap.Sample(_metallicRoughnessMap_sampler, inUV).xyz, 1.0f);
    outPosition = float4(inWorldPosition, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    inUV = stage_input.inUV;
    inTangent = stage_input.inTangent;
    inBitangent = stage_input.inBitangent;
    inNormal = stage_input.inNormal;
    inWorldPosition = stage_input.inWorldPosition;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outAlbedo = outAlbedo;
    stage_output.outNormal = outNormal;
    stage_output.outMRA = outMRA;
    stage_output.outPosition = outPosition;
    return stage_output;
}
