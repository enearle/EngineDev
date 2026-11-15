Texture2D TestImage : register(t0, space1);
SamplerState pointClampSampler : register(s1, space0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

float4 main(VSOutput input) : SV_TARGET
{
    return TestImage.Sample(pointClampSampler, input.uv);
}
