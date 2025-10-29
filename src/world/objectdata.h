#ifndef OBJECTDATA_H
#define OBJECTDATA_H

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../graphics/align_check.h"
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
    /// @brief Time between every frame.
    GPU_EXPOSED(float, frameInterval) = 0;
};

/**
 * @brief Data concerning the rendering of an object.
 */
struct ObjectData
{
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

class Chunk2
{
public:
    std::vector<std::span<ObjectData>> references{};
    std::vector<ObjectData> objects{};
    std::vector<Physics::Entity> entities{};
};

struct __Check
{
    __Check() = delete;

    static_assert(sizeof(ObjectData) == (4 + 4 + 4 + 4 + 16 + 64), "");
    static_assert(offsetof(ObjectData, objId) == 0, "objId offset mismatch");
    static_assert(offsetof(ObjectData, verticesId) == 4, "verticesId offset mismatch");
    static_assert(offsetof(ObjectData, animationId) == 8, "animationId offset mismatch");
    static_assert(offsetof(ObjectData, animationTime) == 12, "animationTime offset mismatch");
    static_assert(offsetof(ObjectData, position) == 16, "position offset mismatch");
    static_assert(offsetof(ObjectData, transform) == 32, "transform offset mismatch");
};

} // namespace Graphics

#endif // OBJECTDATA_H
