#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBinormal;
layout(location = 4) in vec2 inUV;

layout(set = 0, binding = 0, row_major) uniform VPData {
    mat4 viewProjection;
    vec4 cameraPosition;
} vpData;

layout(push_constant, row_major) uniform ModelData {
    mat4 model;
    mat4 normal;

} modelData;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBinormal;
layout(location = 4) out vec2 outUV;

void main() {
    vec4 worldPosition = vec4(inPosition, 1.0) * modelData.model;
    mat3 normalMatrix = mat3(modelData.normal);
    
    gl_Position = worldPosition * vpData.viewProjection;
    outWorldPosition = worldPosition.xyz;
    outNormal   = normalize(inNormal * normalMatrix);
    outTangent  = normalize(inTangent * normalMatrix);
    outBinormal = normalize(inBinormal * normalMatrix);

    outUV = inUV;
}