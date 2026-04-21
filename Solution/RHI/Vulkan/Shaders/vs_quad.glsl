#version 450

layout(location = 0) out vec2 uv;

void main()
{
    const float halfSize = 0.25;

    vec2 pos[6] = vec2[](
    vec2(-halfSize, -halfSize),
    vec2( halfSize, -halfSize),
    vec2(-halfSize,  halfSize),

    vec2(-halfSize,  halfSize),
    vec2( halfSize, -halfSize),
    vec2( halfSize,  halfSize)
    );

    vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
    
    uv  = (p / halfSize) * 0.5 + 0.5;
}