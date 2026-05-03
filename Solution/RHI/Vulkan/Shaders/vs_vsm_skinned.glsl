#version 450
#extension GL_EXT_multiview : enable
#define MAX_BONES 128

layout(location = 0) in vec3 inPosition;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIndices;

struct Light
{
    vec3 Position;
    vec3 Colour;
    float Intensity;
    float Radius;
    uint Type;
    float Angle;
    uint ShadowIndex;
};

layout(set = 0, binding = 0, row_major) uniform LightMatrices {
    mat4 viewProjection[4];
} lightMatrices;

layout(set = 1, binding = 0) uniform LightData
{
    Light Lights[4];
} lightData;

layout(push_constant, row_major) uniform ModelData {
    mat4 model;
    mat4 normal;
} modelData;

layout(set = 16, binding = 0, row_major) uniform BoneBuffer {
    mat4 bones[MAX_BONES];
} BoneData;

layout(location = 0) out float outDepth;
void main() {
    
    vec4 skinnedPos = vec4(0.0);
    for (int i = 0; i < 4; i++) 
    {
        float weight = inBoneWeights[i];
        if (weight > 0.0) {
            uint boneIndex = inBoneIndices[i];
            mat4 boneTransform = BoneData.bones[boneIndex];
    
            skinnedPos += weight * (vec4(inPosition, 1.0) * boneTransform);
        }
    }
    
    vec4 worldPosition = skinnedPos * modelData.model;
    vec4 clipPos = worldPosition * lightMatrices.viewProjection[gl_ViewIndex];
    gl_Position = clipPos;

    float linearZ = clipPos.w;
    outDepth = linearZ / lightData.Lights[gl_ViewIndex].Radius;
}
