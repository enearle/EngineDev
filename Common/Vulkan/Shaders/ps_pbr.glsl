#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBinormal;
layout(location = 4) in vec2 inUV;

// PBR textures
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

// G-Buffer outputs (matching your lighting pass expectations)
layout(location = 0) out vec4 outAlbedo;           // R8G8B8A8_UNORM
layout(location = 1) out vec4 outNormal;           // R16G16B16A16_FLOAT
layout(location = 2) out vec4 outMaterial;         // R8G8B8A8_UNORM (Metal, Rough, AO)
layout(location = 3) out vec4 outPosition;         // R32G32B32A32_FLOAT

void main() {
    // Sample albedo texture
    vec3 albedo = texture(albedoMap, inUV).rgb;

    // Sample normal map and convert from [0,1] to [-1,1]
    vec3 tangentNormal = texture(normalMap, inUV).rgb * 2.0 - 1.0;

    // Construct TBN matrix to transform normal from tangent space to world space
    mat3 TBN = mat3(
    normalize(inTangent),
    normalize(inBinormal),
    normalize(inNormal)
    );

    // Transform normal to world space
    vec3 worldNormal = normalize(TBN * tangentNormal);

    // Sample metallic-roughness map
    // Standard PBR convention: R = metallic, G = roughness, B = ambient occlusion
    vec3 metallicRoughnessAO = texture(metallicRoughnessMap, inUV).rgb;

    // Output to G-Buffer targets
    outAlbedo = vec4(albedo, 1.0);
    outNormal = vec4(worldNormal, 1.0);
    outMaterial = vec4(metallicRoughnessAO, 1.0);  // R=metallic, G=roughness, B=AO
    outPosition = vec4(inWorldPosition, 1.0);

}