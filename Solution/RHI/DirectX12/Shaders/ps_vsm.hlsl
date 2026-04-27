Texture2D albedoMap : register(t0, space2);
SamplerState linearSampler : register(s0, space0);

struct PSOutput
{
    float2 depth : SV_TARGET0;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSOutput main(PSInput input)
{
    PSOutput output;
    output.depth.x = input.position.z;
    output.depth.y = output.depth.x * output.depth.x;
    return output;
}
    
    