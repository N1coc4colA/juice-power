#include "src/graphics/types.h"

namespace Graphics {

/**
 * @brief Compile-time and runtime checks for GPU struct alignment assumptions.
 */
class AlignmentChecker
{
public:
    AlignmentChecker() = delete;

    // This is used for Graphics::Alignment::Padding class.
    // 9 is just a random value.
    static_assert(sizeof(std::byte) * 9 == sizeof(std::array<std::byte, 9>),
                  "The size of an std::array should be the same as a []. This breaks mem layout.");

    static_assert(sizeof(Vertex) == 48, "Vertex size mismatch");
    static_assert(offsetof(Vertex, uv) == 16, "uv offset mismatch");
    static_assert(offsetof(Vertex, normal) == 32, "normal offset mismatch");

    static_assert(sizeof(GPUDrawPushConstants2) == 64 + 8 + 8 + 8 + /*padding*/ 8, "GPUDrawPushConstants2 size mismatch");
    static_assert(offsetof(GPUDrawPushConstants2, vertexBuffer) == 64, "vertexBuffer offset mismatch");
    static_assert(offsetof(GPUDrawPushConstants2, animationBuffer) == 72, "animationBuffer offset mismatch");
    static_assert(offsetof(GPUDrawPushConstants2, objectsBuffer) == 80, "objectsBuffer offset mismatch");

    static_assert(offsetof(GPUDrawLinePushConstants, vertexBuffer) == 80, "vertexBuffer offset mismatch");

    static_assert(offsetof(GPUDrawPointPushConstants, color) == 16, "color offset mismatch");

    static_assert(sizeof(ObjectData) == 4 + 4 + 4 + 4 + 16 + 64, "ObjectData size mismatch");
    static_assert(offsetof(ObjectData, objId) == 0, "objId offset mismatch");
    static_assert(offsetof(ObjectData, verticesId) == 4, "verticesId offset mismatch");
    static_assert(offsetof(ObjectData, animationId) == 8, "animationId offset mismatch");
    static_assert(offsetof(ObjectData, animationTime) == 12, "animationTime offset mismatch");
    static_assert(offsetof(ObjectData, position) == 16, "position offset mismatch");
    static_assert(offsetof(ObjectData, transform) == 32, "transform offset mismatch");

    static_assert(sizeof(AnimationData) == 4 + 2 + 2 + 2 + 2 + 4 + 16, "AnimationData size mismatch");
    static_assert(offsetof(AnimationData, frameInterval) == 12, "frameInterval offset mismatch");
    static_assert(offsetof(AnimationData, imageInfo) == 16, "imageInfo offset mismatch");
    static_assert(offsetof(AnimationData, imageId) == 0, "imageId offset mismatch");
    static_assert(offsetof(AnimationData, gridRows) == 4, "gridRows offset mismatch");
    static_assert(offsetof(AnimationData, gridColumns) == 6, "gridColumns offset mismatch");
    static_assert(offsetof(AnimationData, framesCount) == 8, "framesCount offset mismatch");
};

} // namespace Graphics
