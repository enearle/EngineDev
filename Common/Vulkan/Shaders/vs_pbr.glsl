#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBinormal;
layout(location = 4) in vec2 inUV;

layout(push_constant, row_major) uniform MVPData {
    mat4 model;
    mat4 normal;
    mat4 viewProjection;
    vec4 cameraPosition;
} mvpData;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBinormal;
layout(location = 4) out vec2 outUV;

void main() {
    vec4 worldPosition = vec4(inPosition, 1.0) * mvpData.model;
    mat3 normalMatrix = mat3(mvpData.normal);
    
    gl_Position = worldPosition * mvpData.viewProjection;
    outWorldPosition = worldPosition.xyz;
    outNormal   = normalize(inNormal * normalMatrix);
    outTangent  = normalize(inTangent * normalMatrix);
    outBinormal = normalize(inBinormal * normalMatrix);

    outUV = inUV;
}