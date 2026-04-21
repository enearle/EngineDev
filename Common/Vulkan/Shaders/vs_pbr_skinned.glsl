#version 450
#define MAX_BONES 128

layout(push_constant, row_major) uniform RootConstants {
    mat4 model;
    mat4 normal;
} ModelData;

layout(set = 0, binding = 0, row_major) uniform VPData {
    mat4 viewProjection;
    vec4 cameraPosition;
} vpData;

layout(set = 16, binding = 0, row_major) uniform BoneBuffer {
    mat4 bones[MAX_BONES];
} BoneData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBinormal;
layout(location = 4) in vec2 inUV;
layout(location = 5) in vec4 inBoneWeights;
layout(location = 6) in uvec4 inBoneIndices;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBinormal;
layout(location = 4) out vec2 outUV;

void main() {
    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    vec3 skinnedTangent = vec3(0.0);
    vec3 skinnedBinormal = vec3(0.0);

    for (int i = 0; i < 4; i++) {
        float weight = inBoneWeights[i];
        if (weight > 0.0) {
            uint boneIndex = inBoneIndices[i];
            mat4 boneTransform = BoneData.bones[boneIndex];

            skinnedPos += weight * (vec4(inPosition, 1.0) * boneTransform);
            skinnedNormal += weight * (inNormal * mat3(boneTransform));
            skinnedTangent += weight * (inTangent * mat3(boneTransform));
            skinnedBinormal += weight * (inBinormal * mat3(boneTransform));
        }
    }

    vec4 worldPos = skinnedPos * ModelData.model;
    outWorldPosition = worldPos.xyz;
    gl_Position = worldPos * vpData.viewProjection;
    
    mat3 modelRotation = mat3(ModelData.model); // this is the only thing that worked here, feels wrong but will come back to it when offsets are properly coded on engine side
    outNormal = normalize(skinnedNormal * modelRotation);
    outTangent = normalize(skinnedTangent * modelRotation);
    outBinormal = normalize(skinnedBinormal * modelRotation);

    outUV = inUV;
}