#ifndef CHUNK_H
#define CHUNK_H

#include <glm/glm.hpp>

#include <vector>

#include "src/physics/entity.h"

namespace World
{

/**
 * @brief Logical container for entities and render instances inside one scene region.
 */
class Chunk
{
public:
	/* Objects' Information */

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
    std::vector<glm::vec3> positions{};
    /**
	 * @brief Objects Transformation
	 */
	std::vector<glm::mat4> transforms {};

    /// @brief Physics entities for this chunk.
	std::vector<Physics::Entity> entities {};

    /// @brief Current animation frame
    std::vector<float> animFrames;
};
}


#endif
