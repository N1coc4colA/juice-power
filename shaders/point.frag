#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    const float radius = 0.5;
    const float dist = length(gl_PointCoord - vec2(radius));
    if (dist > radius) {
        discard;
    }

    // Smooth edge
    const float alpha = 1.0 - smoothstep(0.4, 0.5, dist);
    outColor = vec4(inColor.rgb, alpha);
}
