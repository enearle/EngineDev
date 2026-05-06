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

cbuffer LightMatrices : register(b0, space0)
{
    row_major float4x4 lightMatrices_viewProjection[4] : packoffset(c0);
};

cbuffer LightData : register(b0, space1)
{
    Light lightData_Lights[4] : packoffset(c0);
};

cbuffer ModelData : register(b0, space999)
{
    row_major float4x4 modelData_model : packoffset(c0);
    row_major float4x4 modelData_normal : packoffset(c4);
};


static float4 gl_Position;
static uint gl_ViewIndex;
static float3 inPosition;
static float outDepth;

struct SPIRV_Cross_Input
{
    float3 inPosition : POSITION;
    uint gl_ViewIndex : SV_ViewID;
};

struct SPIRV_Cross_Output
{
    float outDepth : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    float4 _54 = mul(mul(float4(inPosition, 1.0f), modelData_model), lightMatrices_viewProjection[gl_ViewIndex]);
    gl_Position = _54;
    outDepth = _54.w / lightData_Lights[gl_ViewIndex].Radius;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_ViewIndex = stage_input.gl_ViewIndex;
    inPosition = stage_input.inPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.outDepth = outDepth;
    return stage_output;
}
