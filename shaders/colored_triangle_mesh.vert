#version 450
#extension GL_EXT_buffer_reference     : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_debug_printf         : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require // temporary [TODO] Change uint16_t to uint32_t.


layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;


layout(std430) struct Vertex {
    vec3 position;
    vec2 uv;
    vec3 normal;
};

layout(std430) struct ObjectData {
    uint objId;
    uint verticesId;
    uint animationId;
    float animationTime;
    vec4 position;
    mat4 transform;
};

layout(std430) struct AnimationData {
    uint imageId;
    uint16_t gridRows;
    uint16_t gridColumns;
    uint16_t framesCount;
    float frameInterval;
};


layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer ObjectDataBuffer {
    ObjectData objects[];
};

layout(buffer_reference, std430) readonly buffer AnimationBuffer {
    AnimationData animations[];
};


//push constants block
layout(push_constant) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	AnimationBuffer animationBuffer;
    ObjectDataBuffer objectBuffer;
} PushConstants;


void main()
{
    const ObjectData objData = PushConstants.objectBuffer.objects[gl_InstanceIndex];
    const AnimationData anim = PushConstants.animationBuffer.animations[objData.animationId];

    // Get vertex data
    const Vertex v = PushConstants.vertexBuffer.vertices[objData.verticesId * 4 + gl_VertexIndex];

    // Calculate frame
    const int frameIndex = int(objData.animationTime / anim.frameInterval) % int(anim.framesCount);
    const int col = frameIndex % int(anim.gridColumns);
    const int row = frameIndex / int(anim.gridColumns);

    // Calculate UV coordinates for current frame
    const vec2 frameSize = vec2(1.0 / anim.gridColumns, 1.0 / anim.gridRows);
    const vec2 frameOffset = vec2(col, row) * frameSize;
    const vec2 uv = v.uv * frameSize + frameOffset;

    gl_Position = PushConstants.render_matrix * vec4(v.position + objData.position.xyz, 1.0);
    //gl_Position = PushConstants.render_matrix * objData.transform * vec4(v.position, 1.0);
    //gl_Position = PushConstants.render_matrix * vec4(v.position, 1.0);
    outColor = vec3(1.f, 1.f, 1.f);
    outUV = uv;

    //debugPrintfEXT("Vertex of %d: %d, inst. index: %d, src_pos: %f %f, gl_pos: %f %f %f, src_uv: %f %f, uv: %f %f, animation %d",
    //               objData.objId, objData.verticesId, gl_InstanceIndex, v.position.x, v.position.y, gl_Position.x, gl_Position.y, gl_Position.z, v.uv.x, v.uv.y, uv.x, uv.y, objData.animationId);
}
