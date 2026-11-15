// Mirrors ComputeTestTexel in Renderer/Passes/ComputeTestPass.cpp.
[[vk::image_format("rgba8")]]
RWTexture2D<float4> Output : register(u0, space1);

struct TestConstants
{
    uint Frame;
    uint3 Padding;
};

[[vk::push_constant]]
ConstantBuffer<TestConstants> Constants : register(b0, space999);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint frame = Constants.Frame;
    uint3 bytes = uint3((id.x + frame) & 255, (id.y + frame * 3) & 255, frame & 255);
    Output[id.xy] = float4(bytes / 255.0, 1.0);
}
