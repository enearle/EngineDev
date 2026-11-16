// Marks every feedback block as asking for nothing (texture 0xFF) before the G-buffer pass writes it.
RWStructuredBuffer<uint2> Feedback : register(u0, space1);

struct ClearConstants
{
    uint Count;
    uint3 Padding;
};

[[vk::push_constant]]
ConstantBuffer<ClearConstants> Constants : register(b0, space999);

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x < Constants.Count)
        Feedback[id.x] = uint2(0xFFFFFFFF, 0xFFFFFFFF);
}
