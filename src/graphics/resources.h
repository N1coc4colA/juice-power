#ifndef GRAPHICS_RESOURCES_H
#define GRAPHICS_RESOURCES_H

#include <vector>
#include <string>

#include <glm/glm.hpp>

#include "allocatedimage.h"
#include "types.h"


namespace Graphics
{

class Engine;


class Resources
{
public:
	/* Objects' Descriptions */
	using Vertices = union { Vertex data[4]; };

	/// @brief Vertices for each element.
	std::vector<Vertices> vertices {};

	/**
	 * @brief Surfaces of the different models.
	 * @note Used by the physics engine, never used in graphics.
	 */
	std::vector<std::vector<glm::vec4>> surfaces {};

	/// @brief Images of the different models.
	std::vector<AllocatedImage> images {};

	/**
	 * @brief Types of the models.
	 * @note Used by the physics engine, never used in graphics.
	 */
	std::vector<uint32_t> types {};

	/// @brief Mesh buffers for vertices.
	GPUMeshBuffers meshBuffers {};

	void build(Engine &engine);
};


}


#endif // GRAPHICS_RESOURCES_H
