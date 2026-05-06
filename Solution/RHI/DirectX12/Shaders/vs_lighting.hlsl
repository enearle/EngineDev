static const float2 _22[6] = { (-1.0f).xx, float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(1.0f, -1.0f), float2(-1.0f, 1.0f), 1.0f.xx };

static float4 gl_Position;
static int gl_VertexIndex;
static float2 outUV;

struct SPIRV_Cross_Input
{
    uint gl_VertexIndex : SV_VertexID;
};

struct SPIRV_Cross_Output
{
    float2 outUV : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    gl_Position = float4(_22[gl_VertexIndex], 0.0f, 1.0f);
    outUV = ((_22[gl_VertexIndex] * float2(1.0f, -1.0f)) + 1.0f.xx) * 0.5f;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_VertexIndex = int(stage_input.gl_VertexIndex);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.outUV = outUV;
    return stage_output;
}
