struct RootConstants
{
    float4x4 model;
    float4x4 normal;
};
ConstantBuffer<RootConstants> ModelData : register(b0, space999);

struct CBVBuffer
{
    float4x4 viewProjection;
    float4 cameraPosition;
};
ConstantBuffer<CBVBuffer> VPData : register(b0, space0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    float4 worldPos = mul(ModelData.model, float4(input.position, 1.0));
    output.worldPosition = worldPos.xyz;
    output.position = mul(VPData.viewProjection, worldPos);
    
    float3x3 normalMatrix = (float3x3)ModelData.normal;
    output.normal = normalize(mul(normalMatrix, input.normal));
    output.tangent = normalize(mul(normalMatrix, input.tangent));
    output.binormal = normalize(mul(normalMatrix, input.binormal));
    
    output.uv = input.uv;
    
    return output;
}