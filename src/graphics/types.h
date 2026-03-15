#ifndef GRAPHICS_TYPES_H
#define GRAPHICS_TYPES_H

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#include "align_check.h"

namespace Graphics
{

/**
 * @brief Push constants for compute shader operations
 * @note Contains four vec4 for flexible compute shader parameter passing
 */
struct ComputePushConstants
{
    GPU_EXPOSED(glm::vec4, data1) {};
    GPU_EXPOSED(glm::vec4, data2) {};
    GPU_EXPOSED(glm::vec4, data3) {};
    GPU_EXPOSED(glm::vec4, data4) {};
};

struct ComputeEffect
{
	const char *name = "";

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;

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

struct EngineStats
{
	float frameTime = 0.f;
    int triangleCount = 0;
    int drawcallCount = 0;
    float sceneUpdateTime = 0.f;
    float meshDrawTime = 0.f;
};

/**
 * @brief Vertex format for mesh rendering
 */
struct Vertex
{
    GPU_EXPOSED(glm::vec3, position) {};
    GPUChecks::Padding<4> _pad0{};
    GPU_EXPOSED(glm::vec2, uv) {};
    GPUChecks::Padding<8> _pad1{};
    GPU_EXPOSED(glm::vec3, normal) {};
    GPUChecks::Padding<4> _pad2{};
};

struct LineVertex
{
    GPU_EXPOSED(glm::vec2, position) {};
};

/// @brief GPU resources for a mesh
struct GPUMeshBuffers
{
	AllocatedBuffer indexBuffer {};
	AllocatedBuffer vertexBuffer {};
	VkDeviceAddress vertexBufferAddress = 0;
};

struct GPUObjectDataBuffer
{
    AllocatedBuffer buffer;
    VkDeviceAddress deviceAddress;
};

struct GPULineBuffers
{
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer vertexBuffer{};
    VkDeviceAddress vertexBufferAddress = 0;
};

struct GPUAnimationBuffers
{
    AllocatedBuffer animationBuffer{};
    VkDeviceAddress animationBufferAddress = 0;
};

struct GPUPointBuffers
{
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer vertexBuffer{};
    VkDeviceAddress vertexBufferAddress = 0;
};

/// @brief Push constants for indirect mesh drawing
struct GPUDrawPushConstants2
{
    GPU_EXPOSED(glm::mat4, worldMatrix) {};
    GPU_EXPOSED(VkDeviceAddress, vertexBuffer) = 0;
    GPU_EXPOSED(VkDeviceAddress, animationBuffer) = 0;
    GPU_EXPOSED(VkDeviceAddress, objectsBuffer) = 0;
};

struct GPUDrawLinePushConstants
{
    GPU_EXPOSED(glm::mat4, worldMatrix) {};
    GPU_EXPOSED(glm::vec3, color) {};
    GPUChecks::Padding<4> _pad0;
    GPU_EXPOSED(VkDeviceAddress, vertexBuffer) = 0;
};

struct GPUDrawPointPushConstants
{
    GPU_EXPOSED(glm::vec2, pos) {};
    GPUChecks::Padding<8> _pad0;
    GPU_EXPOSED(glm::vec4, color) {};
};

namespace __Types {
struct __Check
{
    __Check() = delete;

    static_assert(sizeof(Vertex) == 48, "Vertex size mismatch");
    static_assert(offsetof(Vertex, uv) == 16, "uv offset mismatch");
    static_assert(offsetof(Vertex, normal) == 32, "normal offset mismatch");

    static_assert(sizeof(GPUDrawPushConstants2) == 64 + 8 + 8 + 8 + /*padding*/ 8, "GPUDrawPushConstants2 size mismatch");
    static_assert(offsetof(GPUDrawPushConstants2, vertexBuffer) == 64, "vertexBuffer offset mismatch");
    static_assert(offsetof(GPUDrawPushConstants2, animationBuffer) == 72, "animationBuffer offset mismatch");
    static_assert(offsetof(GPUDrawPushConstants2, objectsBuffer) == 80, "objectsBuffer offset mismatch");

    static_assert(offsetof(GPUDrawLinePushConstants, vertexBuffer) == 80, "vertexBuffer offset mismatch");

    static_assert(offsetof(GPUDrawPointPushConstants, color) == 16, "color offset mismatch");
};

} // namespace __Types
} // namespace Graphics

#endif // GRAPHICS_TYPES_H
