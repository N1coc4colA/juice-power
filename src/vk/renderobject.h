#ifndef RENDEROBJECT_H
#define RENDEROBJECT_H

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

#include <vulkan/vulkan_core.h>


struct MaterialInstance;


struct RenderObject
{
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance *material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext
{
	std::vector<RenderObject> OpaqueSurfaces;
};


#endif // RENDEROBJECT_H
