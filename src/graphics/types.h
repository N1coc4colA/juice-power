#ifndef GRAPHICS_TYPES_H
#define GRAPHICS_TYPES_H

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


namespace Graphics
{

struct MeshPushConstants
{
	glm::vec4 data {};
	glm::mat4 render_matrix {};
};

/**
 * @brief Push constants for compute shader operations
 * @note Contains four vec4 for flexible compute shader parameter passing
 */
struct ComputePushConstants
{
	glm::vec4 data1 {};
	glm::vec4 data2 {};
	glm::vec4 data3 {};
	glm::vec4 data4 {};
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
	glm::vec3 position {};
	float uv_x = 0.f;
	glm::vec3 normal {};
	float uv_y = 0.f;
	glm::vec4 color {};
};

struct LineVertex
{
    glm::vec2 position;
};

/// @brief GPU resources for a mesh
struct GPUMeshBuffers
{
	AllocatedBuffer indexBuffer {};
	AllocatedBuffer vertexBuffer {};
	VkDeviceAddress vertexBufferAddress = 0;
};

struct GPULineBuffers
{
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer vertexBuffer{};
    VkDeviceAddress vertexBufferAddress = 0;
};

struct GPUPointBuffers
{
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer vertexBuffer{};
    VkDeviceAddress vertexBufferAddress = 0;
};

/// @brief Push constants for indirect mesh drawing
struct GPUDrawPushConstants
{
    float animationTime{};
    float frameInterval = 0.01;

    glm::uint16_t gridColumns = 1;
    glm::uint16_t gridRows = 1;
    glm::uint16_t framesCount = 0;

    glm::mat4 worldMatrix{};
    VkDeviceAddress vertexBuffer = 0;
};

struct GPUDrawLinePushConstants
{
    glm::mat4 worldMatrix{};
    glm::vec3 color{};
    VkDeviceAddress vertexBuffer = 0;
};

struct GPUDrawPointPushConstants
{
    glm::vec2 pos{};
    glm::vec4 color{};
};

struct GPUSceneData
{
	glm::mat4 view {};
	glm::mat4 proj {};
	glm::mat4 viewproj {};
	glm::vec4 ambientColor {};
	glm::vec4 sunlightDirection {}; // w for sun power
	glm::vec4 sunlightColor {};
};


}


#endif // GRAPHICS_TYPES_H
