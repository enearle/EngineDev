#version 450

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 tex = texture(uTexture, uv);
    outColor = tex;
}