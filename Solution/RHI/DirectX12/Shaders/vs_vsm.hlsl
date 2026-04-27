struct RootConstants
{
    float4x4 model;
    float4x4 normal;
};
ConstantBuffer<RootConstants> ModelData : register(b0, space0);

struct CBVBuffer
{
    float4x4 viewProjection [4];
};
ConstantBuffer<CBVBuffer> LightData : register(b0, space1);

struct VSOutput {
    float4 position : SV_Position;
};

VSOutput main(float3 inPosition : POSITION, uint viewID : SV_ViewID) {
    VSOutput output;
    float4 worldPosition = mul(ModelData.model, float4(inPosition, 1.0));
    output.position = mul(LightData.viewProjection[viewID], worldPosition);
    return output;
}