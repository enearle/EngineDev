#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 inPosition;

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

layout(set = 0, binding = 0) uniform LightMatrices {
    mat4 viewProjection[4];
} lightMatrices;

layout(set = 1, binding = 0) uniform LightData
{
    Light Lights[4];
} lightData;

layout(push_constant) uniform ModelData {
    mat4 model;
    mat4 normal;
} modelData;

layout(location = 0) out float outDepth;

void main() {
    vec4 worldPosition = modelData.model * vec4(inPosition, 1.0);
    vec4 clipPos = lightMatrices.viewProjection[gl_ViewIndex] * worldPosition;
    gl_Position = clipPos;

    float linearZ = clipPos.w;
    outDepth = linearZ / lightData.Lights[gl_ViewIndex].Radius;
}
