struct VSInput
{
    float3 position     : POSITION;
    float3 normal       : NORMAL;
    float3 tangent      : TANGENT;
    float3 bitangent    : BINORMAL;
    float2 UV           : TEXCOORD;
};

struct VSOutput
{
    float4 position     : SV_Position;
    float3 worldPos     : TEXCOORD0;
    float3 normal       : TEXCOORD1;
    float3 tangent      : TEXCOORD2;
    float3 bitangent    : TEXCOORD3;
    float2 uv           : TEXCOORD4;
};

cbuffer VPData : register(b0)
{
    float4x4 model;
    float4x4 normal;
    float4x4 viewProjection;
    float4 cameraPos;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    float4 worldPos = mul(float4(input.position, 1), model);
    output.worldPos = worldPos.xyz;
    output.position = mul(worldPos, viewProjection);
    
    float3x3 normalMatrix = (float3x3)normal;
    
    output.normal   = mul(input.normal, normalMatrix);
    output.tangent  = mul(input.tangent, normalMatrix);
    output.bitangent = mul(input.bitangent, normalMatrix);

    output.uv = input.UV;
    return output;
}