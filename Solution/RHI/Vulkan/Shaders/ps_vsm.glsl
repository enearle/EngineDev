#version 450

layout(location = 0) in float inDepth;
layout(location = 0) out vec2 depth;

void main() {
    depth.x = inDepth;
    depth.y = depth.x * depth.x;
}