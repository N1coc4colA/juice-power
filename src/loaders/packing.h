#ifndef PACKING_H
#define PACKING_H

#include <cstdint>
#include <vector>

/// @brief stb_image byte type alias.
using stbi_uc = unsigned char;

/**
 * @brief Image metadata and placement result for atlas packing.
 */
struct ImageInfo
{
    /// @brief Source image width in pixels.
    int width = -1;
    /// @brief Source image height in pixels.
    int height = -1;
    /// @brief Target frame index after packing.
    int frameId = -1;
    /// @brief Top-left X placement in frame in pixels.
    int x = -1;
    /// @brief Top-left Y placement in frame in pixels.
    int y = -1;
    /// @brief Packing status flag.
    int packed = -1;
    /// @brief Pointer to raw image data.
    stbi_uc *imgData = nullptr;
    /// @brief Application image identifier.
    int id = -1;
};

/**
 * @brief One output atlas frame metadata.
 */
struct Frame
{
    /// @brief Frame width in pixels.
    int w = -1;
    /// @brief Frame height in pixels.
    int h = -1;
    /// @brief Number of images placed in this frame.
    int imagesCount = -1;
};

/**
 * @brief Packs images into one or more frames constrained by max area.
 * @return Number of generated frames or negative error code.
 */
auto packImagesMultiFrame(std::vector<ImageInfo> &images, uint64_t maxArea, std::vector<Frame> &frames) -> int;

#endif // PACKING_H
