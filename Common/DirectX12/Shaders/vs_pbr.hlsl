struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float3 Tangent  : TANGENT;
    float3 Binormal : BINORMAL;
    float2 UV       : TEXCOORD;
};

struct VSOutput
{
    float4 PositionCS : SV_Position;
    float3 NormalWS   : TEXCOORD0;
    float3 TangentWS  : TEXCOORD1;
    float3 BinormalWS : TEXCOORD2;
    float2 UV         : TEXCOORD3;
};

cbuffer CameraCB : register(b0)
{
    float4x4 gViewProj;
    float3   gCameraPosWS;
    float    _pad0;
};

cbuffer ObjectCB : register(b1)
{
    float4x4 gModel;
};

VSOutput VSMain(VSInput input)
{
    VSOutput o;

    float4 posWS = mul(float4(input.Position, 1.0f), gModel);
    o.PositionCS = mul(posWS, gViewProj);

    // Transform basis vectors to world space.
    // NOTE: This assumes gModel has no non-uniform scale. If it does, you want inverse-transpose for normals.
    float3x3 m3 = (float3x3)gModel;

    o.NormalWS   = mul(input.Normal,   m3);
    o.TangentWS  = mul(input.Tangent,  m3);
    o.BinormalWS = mul(input.Binormal, m3);

    o.UV = input.UV;
    return o;
}