#ifndef GRAPHICS_TYPES_H
#define GRAPHICS_TYPES_H

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#include "src/graphics/alignment.h"

namespace Graphics
{

/**
 * @brief Push constants for compute shader operations
 * @note Contains four vec4 for flexible compute shader parameter passing
 */
struct ComputePushConstants
{
    /// @brief User-defined compute payload slot #1.
    GPU_EXPOSED(glm::vec4, data1) {};
    /// @brief User-defined compute payload slot #2.
    GPU_EXPOSED(glm::vec4, data2) {};
    /// @brief User-defined compute payload slot #3.
    GPU_EXPOSED(glm::vec4, data3) {};
    /// @brief User-defined compute payload slot #4.
    GPU_EXPOSED(glm::vec4, data4) {};
};

/**
 * @brief Named compute pipeline and push constant payload.
 */
struct ComputeEffect
{
	/// @brief Debug/display name of the effect.
	const char *name = "";

	/// @brief Vulkan compute pipeline handle.
	VkPipeline pipeline = VK_NULL_HANDLE;
	/// @brief Pipeline layout used by the compute pipeline.
	VkPipelineLayout layout = VK_NULL_HANDLE;

	/// @brief Push constant data uploaded before dispatch.
	ComputePushConstants data {};
};

/// @brief Vulkan buffer with VMA memory backing
struct AllocatedBuffer
{
	/// @brief Vulkan buffer handle
	VkBuffer buffer = VK_NULL_HANDLE;

	/// @brief VMA allocation handle
	VmaAllocation allocation = VK_NULL_HANDLE;

	/// @brief Allocation metadata
	VmaAllocationInfo info {.deviceMemory = VK_NULL_HANDLE};
};

/**
 * @brief Per-frame rendering and update timing counters.
 */
struct EngineStats
{
	/// @brief Total frame time in milliseconds.
	float frameTime = 0.f;
    /// @brief Number of triangles rendered.
    int triangleCount = 0;
    /// @brief Number of draw calls emitted.
    int drawcallCount = 0;
    /// @brief Scene update CPU time in milliseconds.
    float sceneUpdateTime = 0.f;
    /// @brief Mesh draw CPU/GPU submission time in milliseconds.
    float meshDrawTime = 0.f;
};

/**
 * @brief Vertex format for mesh rendering
 */
struct Vertex
{
    /// @brief Vertex position in object space.
    GPU_EXPOSED(glm::vec3, position) {};
    /// @brief Padding for std430 alignment.
    Alignment::Padding<4> _pad0{};
    /// @brief Texture coordinates.
    GPU_EXPOSED(glm::vec2, uv) {};
    /// @brief Padding for std430 alignment.
    Alignment::Padding<8> _pad1{};
    /// @brief Vertex normal in object space.
    GPU_EXPOSED(glm::vec3, normal) {};
    /// @brief Padding for std430 alignment.
    Alignment::Padding<4> _pad2{};
};

/**
 * @brief Vertex format used by line rendering.
 */
struct LineVertex
{
    /// @brief 2D position in object/local space.
    GPU_EXPOSED(glm::vec2, position) {};
};

/// @brief GPU resources for a mesh
struct GPUMeshBuffers
{
	/// @brief Index buffer.
	AllocatedBuffer indexBuffer {};
	/// @brief Vertex buffer.
	AllocatedBuffer vertexBuffer {};
	/// @brief Device address of the vertex buffer.
	VkDeviceAddress vertexBufferAddress = 0;
};

/**
 * @brief Storage buffer and address for object draw data.
 */
struct GPUObjectDataBuffer
{
    /// @brief Backing storage buffer.
    AllocatedBuffer buffer{};
    /// @brief Device address used by shaders.
    VkDeviceAddress deviceAddress = 0;
};

/**
 * @brief GPU buffers for line debug rendering.
 */
struct GPULineBuffers
{
    /// @brief Index buffer.
    AllocatedBuffer indexBuffer{};
    /// @brief Vertex buffer.
    AllocatedBuffer vertexBuffer{};
    /// @brief Device address of the vertex buffer.
    VkDeviceAddress vertexBufferAddress = 0;
};

/**
 * @brief GPU buffers for animation metadata.
 */
struct GPUAnimationBuffers
{
    /// @brief Buffer containing packed animation data.
    AllocatedBuffer animationBuffer{};
    /// @brief Device address of animation buffer.
    VkDeviceAddress animationBufferAddress = 0;
};

/**
 * @brief GPU buffers for point debug rendering.
 */
struct GPUPointBuffers
{
    /// @brief Index buffer.
    AllocatedBuffer indexBuffer{};
    /// @brief Vertex buffer.
    AllocatedBuffer vertexBuffer{};
    /// @brief Device address of the vertex buffer.
    VkDeviceAddress vertexBufferAddress = 0;
};

/// @brief Push constants for indirect mesh drawing
struct GPUDrawPushConstants2
{
    /// @brief World transform matrix.
    GPU_EXPOSED(glm::mat4, worldMatrix) {};
    /// @brief Vertex buffer device address.
    GPU_EXPOSED(VkDeviceAddress, vertexBuffer) = 0;
    /// @brief Animation buffer device address.
    GPU_EXPOSED(VkDeviceAddress, animationBuffer) = 0;
    /// @brief Object data buffer device address.
    GPU_EXPOSED(VkDeviceAddress, objectsBuffer) = 0;
};

/**
 * @brief Push constants for line rendering.
 */
struct GPUDrawLinePushConstants
{
    /// @brief World transform matrix.
    GPU_EXPOSED(glm::mat4, worldMatrix) {};
    /// @brief Line color.
    GPU_EXPOSED(glm::vec3, color) {};
    /// @brief Padding for std430 alignment.
    Alignment::Padding<4> _pad0 {};
    /// @brief Vertex buffer device address.
    GPU_EXPOSED(VkDeviceAddress, vertexBuffer) = 0;
};

/**
 * @brief Push constants for point rendering.
 */
struct GPUDrawPointPushConstants
{
    /// @brief Point position.
    GPU_EXPOSED(glm::vec2, pos) {};
    /// @brief Padding for std430 alignment.
    Alignment::Padding<8> _pad0 {};
    /// @brief Point color.
    GPU_EXPOSED(glm::vec4, color) {};
};

/**
 * @brief Information about an animation.
 */
struct AnimationData
{
    /// @brief Id of the associated image.
    GPU_EXPOSED(uint32_t, imageId) = 0;

    /// @brief Number of rows in the animation's image.
    GPU_EXPOSED(uint16_t, gridRows) = 0;
    /// @brief Number of columns in the animation's image.
    GPU_EXPOSED(uint16_t, gridColumns) = 0;

    /// @brief Number of sprites in the animation's image.
    GPU_EXPOSED(uint16_t, framesCount) = 0;

    /// @brief Padding for std430 alignment.
    Alignment::Padding<2> _pad0;

    /// @brief Time between every frame.
    GPU_EXPOSED(float, frameInterval) = 0;

    /// @brief (x, y, w, h) of the anim within the image frame.
    GPU_EXPOSED(glm::vec4, imageInfo) { -1, -1, -1, -1 };
};

/**
 * @brief Data concerning the rendering of an object.
 */
struct ObjectData
{
    /// @brief Object unique identifier.
    GPU_EXPOSED(uint32_t, objId) = 0;
    /// @brief Id (offset) of the associated 6 vertices.
    GPU_EXPOSED(uint32_t, verticesId) = 0;
    /// @brief Currently used animation.
    GPU_EXPOSED(uint32_t, animationId) = 0;
    /// @brief Current animation's elapsed time.
    GPU_EXPOSED(float, animationTime) = 0.f;
    /** @brief The object's position within the scene.
     *  The W value of the position should always be 1.f.
     *  @note This is glm:vec4 instead of glm::vec3 for alignment purposes, truth is that it must always be seen as a glm::vec3.
     **/
    GPU_EXPOSED(glm::vec4, position) {};
    /// @brief The object's transformation (size, pos...)
    GPU_EXPOSED(glm::mat4, transform) {};
};

} // namespace Graphics

#endif // GRAPHICS_TYPES_H
