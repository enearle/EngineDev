// One mip from the one above it: a 2 x 2 box filter, clamped at the edge for odd sizes. Colour is
// averaged as linear light.
#include "Colour.hlsli"

Texture2D<float4> Source : register(t0, space1);

[[vk::image_format("rgba8")]]
RWTexture2D<float4> Destination : register(u1, space1);

struct DownsampleConstants
{
    uint2 DestinationSize;
    uint IsColour;
    uint Padding;
};

[[vk::push_constant]]
ConstantBuffer<DownsampleConstants> Constants : register(b0, space999);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= Constants.DestinationSize.x || id.y >= Constants.DestinationSize.y)
        return;

    uint sourceWidth, sourceHeight;
    Source.GetDimensions(sourceWidth, sourceHeight);
    uint2 last = uint2(sourceWidth, sourceHeight) - 1;
    uint2 corner = id.xy * 2;

    float4 texels[4] = {
        Source.Load(int3(min(corner, last), 0)),
        Source.Load(int3(min(corner + uint2(1, 0), last), 0)),
        Source.Load(int3(min(corner + uint2(0, 1), last), 0)),
        Source.Load(int3(min(corner + uint2(1, 1), last), 0)),
    };

    float4 sum = 0.0;
    for (uint i = 0; i < 4; i++)
        sum += Constants.IsColour != 0 ? float4(DecodeSrgb(texels[i].rgb), texels[i].a) : texels[i];
    float4 average = sum * 0.25;
    Destination[id.xy] = Constants.IsColour != 0 ? float4(EncodeSrgb(average.rgb), average.a) : average;
}
