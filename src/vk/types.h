#ifndef TYPES_H
#define TYPES_H

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>


struct MeshPushConstants
{
	glm::vec4 data = {};
	glm::mat4 render_matrix = {};
};

struct ComputePushConstants
{
	glm::vec4 data1 = {};
	glm::vec4 data2 = {};
	glm::vec4 data3 = {};
	glm::vec4 data4 = {};
};

struct ComputeEffect
{
	const char *name = "";

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;

	ComputePushConstants data = {};
};

struct AllocatedBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo info = {.deviceMemory = VK_NULL_HANDLE};
};

struct EngineStats
{
	float frametime = 0.f;
	int triangle_count = 0.f;
	int drawcall_count = 0.f;
	float scene_update_time = 0.f;
	float mesh_draw_time = 0.f;
};

struct Vertex
{
	glm::vec3 position = {};
	float uv_x = 0.f;
	glm::vec3 normal = {};
	float uv_y = 0.f;
	glm::vec4 color = {};
};

// holds the resources needed for a mesh
struct GPUMeshBuffers
{
	AllocatedBuffer indexBuffer = {};
	AllocatedBuffer vertexBuffer = {};
	VkDeviceAddress vertexBufferAddress = 0;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants
{
	glm::mat4 worldMatrix = {};
	VkDeviceAddress vertexBuffer = 0;
};

struct GPUSceneData
{
	glm::mat4 view = {};
	glm::mat4 proj = {};
	glm::mat4 viewproj = {};
	glm::vec4 ambientColor = {};
	glm::vec4 sunlightDirection = {}; // w for sun power
	glm::vec4 sunlightColor = {};
};


#endif // TYPES_H
