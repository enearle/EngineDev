Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 txcoord   : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 sampled = gTexture.Sample(gSampler, input.txcoord);
    return sampled;
    
}
