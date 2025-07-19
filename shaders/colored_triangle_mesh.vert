#version 450
#extension GL_EXT_buffer_reference     : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_debug_printf         : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;

struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer{
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
    float animationTime;
    float frameInterval;

    uint16_t gridColumns;
    uint16_t gridRows;
    uint16_t framesCount;

	mat4 render_matrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main()
{
    const Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    const float animationTime = PushConstants.animationTime;

    const int framesCount = int(PushConstants.framesCount);
    const int columns = int(PushConstants.gridColumns);
    const int rows = int(PushConstants.gridRows);
    const float interval = PushConstants.frameInterval;

    // Calculate current frame
    const int frameIndex = (int(animationTime / interval) % (columns * rows)) % framesCount;
    const int col = frameIndex % int(columns);
    const int row = frameIndex / int(columns);

    // Calculate UV coordinates for current frame
    const vec2 frameSize = vec2(1.0 / columns, 1.0 / rows);
    const vec2 frameOffset = vec2(col, row) * frameSize;
    const vec2 uv = vec2(v.uv_x, v.uv_y) * frameSize + frameOffset;

    gl_Position = PushConstants.render_matrix * vec4(v.position, 1.0f);
    outColor = v.color.xyz;
    outUV = uv;

    //debugPrintfEXT("Input anim data: %f %d %d %d", animationTime, framesCount, columns, rows);
}
