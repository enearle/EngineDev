cbuffer VPData : register(b0, space0)
{
    row_major float4x4 vpData_viewProjection : packoffset(c0);
    float4 vpData_cameraPosition : packoffset(c4);
};

cbuffer ModelData : register(b0, space999)
{
    row_major float4x4 modelData_model : packoffset(c0);
    row_major float4x4 modelData_normal : packoffset(c4);
};


static float4 gl_Position;
static float3 inPosition;
static float3 outWorldPosition;
static float3 outNormal;
static float3 inNormal;
static float3 outTangent;
static float3 inTangent;
static float3 outBinormal;
static float3 inBinormal;
static float2 outUV;
static float2 inUV;

struct SPIRV_Cross_Input
{
    float3 inPosition : POSITION;
    float3 inNormal : NORMAL;
    float3 inTangent : TANGENT;
    float3 inBinormal : BINORMAL;
    float2 inUV : TEXCOORD;
};

struct SPIRV_Cross_Output
{
    float3 outWorldPosition : TEXCOORD0;
    float3 outNormal : TEXCOORD1;
    float3 outTangent : TEXCOORD2;
    float3 outBinormal : TEXCOORD3;
    float2 outUV : TEXCOORD4;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    float4 _52 = mul(float4(inPosition, 1.0f), modelData_model);
    float3x3 _61 = float3x3(modelData_normal[0].xyz, modelData_normal[1].xyz, modelData_normal[2].xyz);
    gl_Position = mul(_52, vpData_viewProjection);
    outWorldPosition = _52.xyz;
    outNormal = normalize(mul(inNormal, _61));
    outTangent = normalize(mul(inTangent, _61));
    outBinormal = normalize(mul(inBinormal, _61));
    outUV = inUV;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    inPosition = stage_input.inPosition;
    inNormal = stage_input.inNormal;
    inTangent = stage_input.inTangent;
    inBinormal = stage_input.inBinormal;
    inUV = stage_input.inUV;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.outWorldPosition = outWorldPosition;
    stage_output.outNormal = outNormal;
    stage_output.outTangent = outTangent;
    stage_output.outBinormal = outBinormal;
    stage_output.outUV = outUV;
    return stage_output;
}
