#ifndef GRAPHICS_RESOURCES_H
#define GRAPHICS_RESOURCES_H

#include <set>
#include <vector>

#include <glm/glm.hpp>

#include "allocatedimage.h"
#include "src/world/objectdata.h"
#include "types.h"

namespace Graphics
{

class Engine;

class Resources2
{
public:
    /* Objects' Descriptions */

    /// @brief Vertices for each element.
    std::vector<Vertex> vertices{};

    /// @brief Images of the different models.
    std::vector<CachedImage> images{};

    /// @brief Animations for the whole scene.
    std::vector<Graphics::AnimationData> animations{};
    GPUAnimationBuffers animationsBuffer{};

    std::unordered_map<uint32_t, uint32_t> groupedImagesMapping{};

    /**
	 * @brief Types of the models.
	 * @note Used by the physics engine, never used in graphics.
	 */
    std::vector<uint32_t> types{};

    /**
	 * @brief Borders delimiting the limits of each resource.
	 * This is determined when loading the resource, and then used by the
	 * physics engine.
	 */
    std::vector<std::vector<glm::vec2>> borders{};
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

    /// @brief Mesh buffers for vertices.
    GPUMeshBuffers meshBuffers{};

    /// @brief To draw lines.
    GPULineBuffers linesBuffer{};

    /// @brief To draw element root point.
    GPUPointBuffers pointsBuffer{};

    void build(Engine &engine);
    void cleanup(Engine &engine);
};
}


#endif // GRAPHICS_RESOURCES_H
