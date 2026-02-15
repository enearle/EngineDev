#version 450

// Vertex inputs
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBinormal;
layout(location = 4) in vec2 inUV;

// Uniform buffers
layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 viewProjection;
} camera;

layout(set = 0, binding = 1) uniform ModelBuffer {
    mat4 model;
} modelData;

// Outputs to fragment shader
layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBinormal;
layout(location = 4) out vec2 outUV;


void main() {
    // Transform position to world space
    vec4 worldPosition = modelData.model * vec4(inPosition, 1.0);
    outWorldPosition = worldPosition.xyz;

    // Transform to clip space
    gl_Position = camera.viewProjection * worldPosition;

    // Transform TBN vectors to world space (assuming uniform scaling)
    // For non-uniform scaling, you'd need the normal matrix (transpose(inverse(model)))
    mat3 normalMatrix = mat3(modelData.model);
    outNormal = normalize(normalMatrix * inNormal);
    outTangent = normalize(normalMatrix * inTangent);
    outBinormal = normalize(normalMatrix * inBinormal);

    // Pass through UVs
    outUV = inUV;
}
