#define MAX_BONES 128

struct RootConstants
{
    float4x4 model;
    float4x4 normal;
};
ConstantBuffer<RootConstants> ModelData : register(b0, space0);

struct BoneBuffer
{
    float4x4 bones[MAX_BONES];
};
ConstantBuffer<BoneBuffer> BoneData : register(b0, space16);

struct CBVBuffer
{
    float4x4 viewProjection;
    float4 cameraPosition;
};
ConstantBuffer<CBVBuffer> VPData : register(b0, space1);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float2 uv : TEXCOORD;
    float4 boneWeights : BLENDWEIGHT;
    uint4 boneIndices : BLENDINDICES;
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
    
    // Skinning
    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNormal = float3(0, 0, 0);
    float3 skinnedTangent = float3(0, 0, 0);
    float3 skinnedBinormal = float3(0, 0, 0);
    
    for (int i = 0; i < 4; i++)
    {
        float weight = input.boneWeights[i];
        if (weight > 0.0)
        {
            uint boneIndex = input.boneIndices[i];
            float4x4 boneTransform = BoneData.bones[boneIndex];
            
            skinnedPos += weight * mul(boneTransform, float4(input.position, 1.0));
            skinnedNormal += weight * mul((float3x3)boneTransform, input.normal);
            skinnedTangent += weight * mul((float3x3)boneTransform, input.tangent);
            skinnedBinormal += weight * mul((float3x3)boneTransform, input.binormal);
        }
    }
    
    float4 worldPos = mul(ModelData.model, skinnedPos);
    output.worldPosition = worldPos.xyz;
    output.position = mul(VPData.viewProjection, worldPos);
    
    float3x3 normalMatrix = (float3x3)ModelData.normal;
    output.normal = normalize(mul(normalMatrix, skinnedNormal));
    output.tangent = normalize(mul(normalMatrix, skinnedTangent));
    output.binormal = normalize(mul(normalMatrix, skinnedBinormal));
    
    output.uv = input.uv;
    
    return output;
}