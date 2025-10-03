#ifndef GRAPHICS_RESOURCES_H
#define GRAPHICS_RESOURCES_H

#include <vector>

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

	/// @brief Images of the different models.
	std::vector<AllocatedImage> images {};

	/**
	 * @brief Types of the models.
	 * @note Used by the physics engine, never used in graphics.
	 */
	std::vector<uint32_t> types {};

	/**
	 * @brief Borders delimiting the limits of each resource.
	 * This is determined when loading the resource, and then used by the
	 * physics engine.
	 */
	std::vector<std::vector<glm::vec2>> borders {};
    std::vector<uint32_t> borderOffsets{};

    /**
	 * @brief Normals of the borders each resource.
	 * This is determined when loading the resource, and then used by the
	 * physics engine.
	 */
    std::vector<std::vector<glm::vec2>> normals{};

    /**
	 * @brief Bounding box of the resource.
	 */
	std::vector<std::tuple<glm::vec2, glm::vec2>> boundingBoxes{};

    /// @brief Spritesheet columns
    std::vector<float> animColumns{};
    /// @brief Spritesheet rows
    std::vector<float> animRows{};
    /// @brief Duration of an animation frame, interval between 2 frames.
    std::vector<float> animInterval{};
    /// @brief Number of frames for the animation.
    std::vector<glm::uint16_t> animFrames{};

    /// @brief Mesh buffers for vertices.
    GPUMeshBuffers meshBuffers{};

    /// @brief to draw lines.
    GPULineBuffers linesBuffer{};

    GPUPointBuffers pointsBuffer{};

    void build(Engine &engine);
    void cleanup(Engine &engine);
};


}


#endif // GRAPHICS_RESOURCES_H
