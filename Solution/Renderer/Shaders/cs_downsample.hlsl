// One mip from the one above it: a 2 x 2 box filter, clamped at the edge for odd sizes.
Texture2D<float4> Source : register(t0, space1);

[[vk::image_format("rgba8")]]
RWTexture2D<float4> Destination : register(u1, space1);

struct DownsampleConstants
{
    uint2 DestinationSize;
    uint2 Padding;
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

    float4 sum = Source.Load(int3(min(corner, last), 0))
               + Source.Load(int3(min(corner + uint2(1, 0), last), 0))
               + Source.Load(int3(min(corner + uint2(0, 1), last), 0))
               + Source.Load(int3(min(corner + uint2(1, 1), last), 0));
    Destination[id.xy] = sum * 0.25;
}
