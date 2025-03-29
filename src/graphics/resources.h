#ifndef RESOURCES_H
#define RESOURCES_H

#include <vector>

#include <glm/glm.hpp>

#include "../vk/allocatedimage.h"
#include "../vk/types.h"


namespace Graphics
{

class Engine;


struct Vertex
{
	glm::vec3 position {};
	glm::vec3 color {};
	glm::vec2 uv {};
};

struct Texture {
	AllocatedImage image;
	VkImageView imageView;
	VkSampler sampler;
};

struct Mesh {
	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;
	uint32_t indexCount;
	Texture texture;
};

class Resources
{
public:
	/* Ojbects' Descriptions */

	std::vector<Vertex[6]> vertices;

	/**
	 * @brief Sizes of the different models.
	 * Mainly used for rendering.
	 */
	std::vector<std::vector<glm::vec3>> geometries {};
	/**
	 * @brief Surfaces of the different models.
	 * Used only for physics.
	 */
	std::vector<std::vector<glm::vec4>> surfaces {};
	/**
	 * @brief Images of the different models.
	 */
	std::vector<AllocatedImage> images {};
	/**
	 * @brief Types of the models.
	 * Used by the physics engine, never used in graphics.
	 */
	uint32_t types = 0;

	void build(Engine &engine);
};

}


#endif // RESOURCES_H
