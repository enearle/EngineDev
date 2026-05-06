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

cbuffer BoneBuffer : register(b0, space16)
{
    row_major float4x4 BoneData_bones[128] : packoffset(c0);
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
static float4 inBoneWeights;
static uint4 inBoneIndices;
static float3 inPosition;
static float outDepth;

struct SPIRV_Cross_Input
{
    float3 inPosition : POSITION;
    float4 inBoneWeights : BLENDWEIGHT;
    uint4 inBoneIndices : BLENDINDICES;
    uint gl_ViewIndex : SV_ViewID;
};

struct SPIRV_Cross_Output
{
    float outDepth : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    float4 skinnedPos = 0.0f.xxxx;
    for (int i = 0; i < 4; i++)
    {
        if (inBoneWeights[i] > 0.0f)
        {
            skinnedPos += (mul(float4(inPosition, 1.0f), BoneData_bones[inBoneIndices[i]]) * inBoneWeights[i]);
        }
    }
    float4 _100 = mul(mul(skinnedPos, modelData_model), lightMatrices_viewProjection[gl_ViewIndex]);
    gl_Position = _100;
    outDepth = _100.w / lightData_Lights[gl_ViewIndex].Radius;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_ViewIndex = stage_input.gl_ViewIndex;
    inBoneWeights = stage_input.inBoneWeights;
    inBoneIndices = stage_input.inBoneIndices;
    inPosition = stage_input.inPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.outDepth = outDepth;
    return stage_output;
}
