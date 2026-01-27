#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    // Define triangle vertices (normalized device coordinates)
    // Top vertex - Red
    vec3 positions[3] = vec3[](
    vec3(0.0f, 0.5f, 0.0f),      // Top
    vec3(-0.5f, -0.5f, 0.0f),    // Bottom left
    vec3(0.5f, -0.5f, 0.0f)      // Bottom right
    );

    // Define colors for each vertex (RGBA)
    vec4 colors[3] = vec4[](
    vec4(1.0f, 0.0f, 0.0f, 1.0f),    // Red
    vec4(0.0f, 1.0f, 0.0f, 1.0f),    // Green
    vec4(0.0f, 0.0f, 1.0f, 1.0f)     // Blue
    );

    // Get vertex data based on vertex index
    vec3 position = positions[gl_VertexIndex];
    fragColor = colors[gl_VertexIndex];

    // Output position in clip space (already in NDC, so convert to homogeneous)
    gl_Position = vec4(position, 1.0f);
}

