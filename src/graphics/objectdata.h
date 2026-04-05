#ifndef OBJECTDATA_H
#define OBJECTDATA_H

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../graphics/alignment.h"
#include "../physics/entity.h"

namespace Graphics {

/**
 * @brief Information about an animation.
 */
struct AnimationData
{
    /// @brief Id of the associated image.
    GPU_EXPOSED(uint32_t, imageId) = 0;

    /// @brief Number of rows in the animation's image.
    GPU_EXPOSED(uint16_t, gridRows) = 0;
    /// @brief Number of columns in the animation's image.
    GPU_EXPOSED(uint16_t, gridColumns) = 0;

    /// @brief Number of sprites in the animation's image.
    GPU_EXPOSED(uint16_t, framesCount) = 0;

    /// @brief Padding for std430 alignment.
    Graphics::Alignment::Padding<2> _pad0;

    /// @brief Time between every frame.
    GPU_EXPOSED(float, frameInterval) = 0;

    /// @brief (x, y, w, h) of the anim within the image frame.
    GPU_EXPOSED(glm::vec4, imageInfo) { -1, -1, -1, -1 };
};

/**
 * @brief Data concerning the rendering of an object.
 */
struct ObjectData
{
    /// @brief Object unique identifier.
    GPU_EXPOSED(uint32_t, objId) = 0;
    /// @brief Id (offset) of the associated 6 vertices.
    GPU_EXPOSED(uint32_t, verticesId) = 0;
    /// @brief Currently used animation.
    GPU_EXPOSED(uint32_t, animationId) = 0;
    /// @brief Current animation's elapsed time.
    GPU_EXPOSED(float, animationTime) = 0.f;
    /** @brief The object's position within the scene.
     *  The W value of the position should always be 1.f.
     *  @note This is glm:vec4 instead of glm::vec3 for alignment purposes, truth is that it must always be seen as a glm::vec3.
     **/
    GPU_EXPOSED(glm::vec4, position) {};
    /// @brief The object's transformation (size, pos...)
    GPU_EXPOSED(glm::mat4, transform) {};
};

/**
 * @brief Runtime grouping of drawable object spans and physics entities.
 */
class Chunk2
{
public:
    /// @brief Groups of contiguous object references sharing draw state.
    std::vector<std::span<ObjectData>> references{};
    /// @brief Owned object data entries.
    std::vector<ObjectData> objects{};
    /// @brief Physics entities associated with objects.
    std::vector<Physics::Entity> entities{};
};

} // namespace Graphics

#endif // OBJECTDATA_H
