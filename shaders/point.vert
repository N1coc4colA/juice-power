#version 450
#extension GL_EXT_buffer_reference     : require
#extension GL_EXT_shader_16bit_storage : require

layout(location = 0) out vec3 outColor;

layout(push_constant) uniform PushConstants {
    vec2 position;
    vec4 color;
} push;

void main() {
    gl_Position = vec4(push.position, 0.0, 1.0);
    gl_PointSize = 10;
    outColor = push.color.rgb;  // Pass color to fragment shader
}
