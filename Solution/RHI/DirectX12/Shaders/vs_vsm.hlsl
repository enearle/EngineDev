struct RootConstants
{
    float4x4 model;
    float4x4 normal;
};
ConstantBuffer<RootConstants> ModelData : register(b0, space999);

struct Light
{
    float3 Position;
    float3 Colour;
    float Intensity;
    float Radius;
    uint Type;
    float Angle;
    uint ShadowIndex;
};

struct CBVBuffer
{
    float4x4 viewProjection [4];
};
ConstantBuffer<CBVBuffer> LightMatrices : register(b0, space0);

cbuffer LightData : register(b0, space1)
{
    Light Lights[4];
};

struct VSOutput {
    float4 position : SV_Position;
    float depth : TEXCOORD0;
};

VSOutput main(float3 inPosition : POSITION, uint viewID : SV_ViewID) {
    VSOutput output;
    float4 worldPosition = mul(ModelData.model, float4(inPosition, 1.0));
    float4 clipPos = mul(LightMatrices.viewProjection[viewID], worldPosition);
    output.position = clipPos;

    float linearZ = clipPos.w;
    output.depth = linearZ / Lights[viewID].Radius;
    return output;
}