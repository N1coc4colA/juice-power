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
    int width;
    /// @brief Source image height in pixels.
    int height;
    /// @brief Target frame index after packing.
    int frame_id;
    /// @brief Top-left X placement in frame in pixels.
    int x;
    /// @brief Top-left Y placement in frame in pixels.
    int y;
    /// @brief Packing status flag.
    int packed;
    /// @brief Pointer to raw image data.
    stbi_uc *imgData = nullptr;
    /// @brief Application image identifier.
    int id;
};

/**
 * @brief One output atlas frame metadata.
 */
struct Frame
{
    /// @brief Frame width in pixels.
    int w;
    /// @brief Frame height in pixels.
    int h;
    /// @brief Number of images placed in this frame.
    int num_images;
};

/**
 * @brief Packs images into one or more frames constrained by max area.
 * @return Number of generated frames or negative error code.
 */
auto packImagesMultiFrame(std::vector<ImageInfo> &images, uint64_t max_area, std::vector<Frame> &frames) -> int;

#endif // PACKING_H
