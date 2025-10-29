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
	int triangleCount = 0.f;
	int drawcallCount = 0.f;
	float sceneUpdateTime = 0.f;
	float meshDrawTime = 0.f;
};

/**
 * @brief Vertex format for mesh rendering
 */
struct Vertex
{
    GPU_EXPOSED(glm::vec3, position) {};
    GPU_EXPOSED(glm::vec2, uv) {};
    GPU_EXPOSED(glm::vec3, normal) {};
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
    GPU_EXPOSED(VkDeviceAddress, vertexBuffer) = 0;
};

struct GPUDrawPointPushConstants
{
    GPU_EXPOSED(glm::vec2, pos) {};
    GPU_EXPOSED(glm::vec4, color) {};
};
}

#endif // GRAPHICS_TYPES_H
