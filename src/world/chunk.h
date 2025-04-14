#ifndef CHUNK_H
#define CHUNK_H

#include <vector>

#include <glm/glm.hpp>


namespace World
{

class Chunk
{
public:
	/* Objects' Informations */

	/**
	 * @brief Objects descriptions
	 * Corresponds to elements in the Descriptions sections.
	 * At runtime, stored in the Resources.
	 */
	std::vector<uint32_t> descriptions {};
	/**
	 * @brief Objects positions
	 * The Z component is used exclusively for drawing.
	 */
	std::vector<glm::vec3> positions {};
	/**
	 * @brief Objects Transformation
	 */
	std::vector<glm::mat4> transforms {};
};


}


#endif
