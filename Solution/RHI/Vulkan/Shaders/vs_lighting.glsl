#version 450

layout(location = 0) out vec2 outUV;
vec2 positions[6] = vec2[](vec2(-1,-1), vec2(-1,1), vec2(1,-1), vec2(1,-1), vec2(-1,1), vec2(1,1));

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0 ,1);
    outUV = (positions[gl_VertexIndex] * vec2(1, -1) + vec2(1,1)) * 0.5f;
}