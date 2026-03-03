#version 450

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBitangent;
layout(location = 4) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D albedoMap;
layout(set = 0, binding = 1) uniform sampler2D normalMap;
layout(set = 0, binding = 2) uniform sampler2D metallicRoughnessMap;

layout(location = 0) out vec4 outAlbedo;  
layout(location = 1) out vec4 outNormal;  
layout(location = 2) out vec4 outMRA;
layout(location = 3) out vec4 outPosition;

void main() {
    vec3 albedo = texture(albedoMap, inUV).rgb;
    vec3 tangentNormal = texture(normalMap, inUV).rgb * 2 - 1;
    vec3 metallicRoughnessAO = texture(metallicRoughnessMap, inUV).rgb;
    
    mat3 TBN = mat3(
        normalize(inTangent),
        -normalize(inBitangent),
        normalize(inNormal)
    );

    vec3 worldNormal = normalize(TBN * tangentNormal);

    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(worldNormal, 1.0);
    outMRA = vec4(metallicRoughnessAO, 1.0);
    outPosition = vec4(inWorldPosition, 1.0);

}