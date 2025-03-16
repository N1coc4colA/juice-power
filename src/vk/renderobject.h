#ifndef RENDEROBJECT_H
#define RENDEROBJECT_H

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

#include <vulkan/vulkan_core.h>


struct MaterialInstance;


struct RenderObject
{
	uint32_t indexCount = -1;
	uint32_t firstIndex = -1;
	VkBuffer indexBuffer = VK_NULL_HANDLE;

	MaterialInstance *material = nullptr;

	glm::mat4 transform = {};
	VkDeviceAddress vertexBufferAddress = 0;
};

struct DrawContext
{
	std::vector<RenderObject> OpaqueSurfaces = {};
};


#endif // RENDEROBJECT_H
