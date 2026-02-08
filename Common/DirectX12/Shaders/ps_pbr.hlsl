struct VSOutput
{
    float4 PositionCS : SV_Position;
    float3 NormalWS   : TEXCOORD0;
    float3 TangentWS  : TEXCOORD1;
    float3 BinormalWS: TEXCOORD2;
    float2 UV         : TEXCOORD3;
};

struct PSOutput
{
    float4 GBuffer0_Albedo : SV_Target0;
    float4 GBuffer1_Normal : SV_Target1;
    float4 GBuffer2_MRA    : SV_Target2;
};

Texture2D gDiffuseTex              : register(t0);
Texture2D gNormalTex               : register(t1);
Texture2D gMetalRoughAOTex         : register(t2);
SamplerState gSamplerLinearWrap    : register(s0);

static float3x3 MakeTBN(float3 n, float3 t, float3 b)
{
    // Orthonormalize a bit to reduce artifacts from imperfect vertex tangents.
    n = normalize(n);
    t = normalize(t - n * dot(n, t));
    b = normalize(b - n * dot(n, b));
    return float3x3(t, b, n); // columns are T, B, N for mul(TBN, normalTS)
}

static float3 UNormToNorm(float3 normalSample)
{
    float3 n = normalSample * 2.0f - 1.0f;
    return normalize(n);
}

static float3 NormToUNorm(float3 n)
{
    return n * 0.5f + 0.5f;
}

PSOutput PSMain(VSOutput i)
{
    PSOutput o;

    float3 albedo = gDiffuseTex.Sample(gSamplerLinearWrap, i.UV).rgb;
    
    float3 normalTS = UNormToNorm(gNormalTex.Sample(gSamplerLinearWrap, i.UV).rgb);
    float3x3 TBN = MakeTBN(i.NormalWS, i.TangentWS, i.BinormalWS);
    float3 normalWS = normalize(mul(normalTS, TBN));

    float3 mra = gMetalRoughAOTex.Sample(gSamplerLinearWrap, i.UV).rgb;

    o.GBuffer0_Albedo = float4(albedo, 1.0f);
    o.GBuffer1_Normal = float4(NormToUNorm(normalWS), 1.0f);
    o.GBuffer2_MRA    = float4(mra, 1.0f);

    return o;
}
