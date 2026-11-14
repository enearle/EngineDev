struct PSOutput
{
    float2 depth : SV_TARGET0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float depth : TEXCOORD0;
};

PSOutput main(PSInput input)
{
    PSOutput output;
    output.depth.x = input.depth;
    output.depth.y = output.depth.x * output.depth.x;
    return output;
}
    
    