#version 450
#extension GL_EXT_buffer_reference     : require
#extension GL_EXT_shader_16bit_storage : require

layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 outColor;

struct Vertex {
	vec2 position;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	vec3 color;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() {
    gl_Position = PushConstants.render_matrix * vec4(PushConstants.vertexBuffer.vertices[gl_VertexIndex].position.xy, 0.0f, 1.0f);
    outColor = PushConstants.color;
}
