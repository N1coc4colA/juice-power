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

	/**
	 * @brief Paths for the images to use.
	 */
	std::vector<std::string> imagePaths {};

	/**
	 * @brief Vertices for each element.
	 */
	std::vector<Vertex[4]> vertices {};

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
	std::vector<uint32_t> types {};

	void build(Engine &engine);
};


}


#endif // GRAPHICS_RESOURCES_H
