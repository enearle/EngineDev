cbuffer BoneBuffer : register(b0, space16)
{
    row_major float4x4 BoneData_bones[128] : packoffset(c0);
};

cbuffer VPData : register(b0, space0)
{
    row_major float4x4 vpData_viewProjection : packoffset(c0);
    float4 vpData_cameraPosition : packoffset(c4);
};

cbuffer RootConstants : register(b0, space999)
{
    row_major float4x4 ModelData_model : packoffset(c0);
    row_major float4x4 ModelData_normal : packoffset(c4);
};


static float4 gl_Position;
static float4 inBoneWeights;
static uint4 inBoneIndices;
static float3 inPosition;
static float3 inNormal;
static float3 inTangent;
static float3 inBinormal;
static float3 outWorldPosition;
static float3 outNormal;
static float3 outTangent;
static float3 outBinormal;
static float2 outUV;
static float2 inUV;

struct SPIRV_Cross_Input
{
    float3 inPosition : POSITION;
    float3 inNormal : NORMAL;
    float3 inTangent : TANGENT;
    float3 inBinormal : BINORMAL;
    float2 inUV : TEXCOORD;
    float4 inBoneWeights : BLENDWEIGHT;
    uint4 inBoneIndices : BLENDINDICES;
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
    float4 skinnedPos = 0.0f.xxxx;
    float3 skinnedNormal = 0.0f.xxx;
    float3 skinnedTangent = 0.0f.xxx;
    float3 skinnedBinormal = 0.0f.xxx;
    for (int i = 0; i < 4; i++)
    {
        if (inBoneWeights[i] > 0.0f)
        {
            skinnedPos += (mul(float4(inPosition, 1.0f), BoneData_bones[inBoneIndices[i]]) * inBoneWeights[i]);
            skinnedNormal += (mul(inNormal, float3x3(BoneData_bones[inBoneIndices[i]][0].xyz, BoneData_bones[inBoneIndices[i]][1].xyz, BoneData_bones[inBoneIndices[i]][2].xyz)) * inBoneWeights[i]);
            skinnedTangent += (mul(inTangent, float3x3(BoneData_bones[inBoneIndices[i]][0].xyz, BoneData_bones[inBoneIndices[i]][1].xyz, BoneData_bones[inBoneIndices[i]][2].xyz)) * inBoneWeights[i]);
            skinnedBinormal += (mul(inBinormal, float3x3(BoneData_bones[inBoneIndices[i]][0].xyz, BoneData_bones[inBoneIndices[i]][1].xyz, BoneData_bones[inBoneIndices[i]][2].xyz)) * inBoneWeights[i]);
        }
    }
    float4 _138 = mul(skinnedPos, ModelData_model);
    outWorldPosition = _138.xyz;
    gl_Position = mul(_138, vpData_viewProjection);
    float3x3 _152 = float3x3(ModelData_model[0].xyz, ModelData_model[1].xyz, ModelData_model[2].xyz);
    outNormal = normalize(mul(skinnedNormal, _152));
    outTangent = normalize(mul(skinnedTangent, _152));
    outBinormal = normalize(mul(skinnedBinormal, _152));
    outUV = inUV;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    inBoneWeights = stage_input.inBoneWeights;
    inBoneIndices = stage_input.inBoneIndices;
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
