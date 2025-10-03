#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0 - inColor.r, 1.0 - inColor.g, 1.0 - inColor.b, 1.0);
}
