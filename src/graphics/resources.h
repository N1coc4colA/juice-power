#ifndef GRAPHICS_RESOURCES_H
#define GRAPHICS_RESOURCES_H

#include <glm/glm.hpp>

#include <memory>
#include <vector>

#include "src/graphics/allocatedimage.h"
#include "src/graphics/types.h"

namespace Graphics
{

class Engine;

/**
 * @brief Aggregates CPU and GPU resource data used by a loaded scene.
 */
class Resources
{
public:
    /* Objects' Descriptions */

    /// @brief Vertices for each element.
    std::vector<Vertex> vertices{};

    /// @brief Images of the different models.
    std::vector<CachedImage> images{};

    /// @brief Animations for the whole scene.
    std::vector<AnimationData> animations{};
    /// @brief GPU-side animation buffer pack.
    GPUAnimationBuffers animationsBuffer{};

    /// @brief Mapping from source image id to grouped image id.
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
    /// @brief Prefix offsets for packed border arrays.
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

    /// @brief Uploads CPU-side resources to GPU buffers and images.
    void build(const std::shared_ptr<Engine> &engine);
    /// @brief Releases GPU resources owned by this object.
    void cleanup(const std::shared_ptr<Engine> &engine);
};
}


#endif // GRAPHICS_RESOURCES_H
