#include "src/loaders/packing.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

// Try packing images into a frame with given dimensions using stb_rect_pack
auto tryPackFrame(std::vector<ImageInfo> &images, const int startIndex, const int frameWidth, const int frameHeight, const int frameId) -> int
{
    const auto count = images.size();
    stbrp_context context{};

    // Allocate nodes for the packing algorithm
    std::vector<stbrp_node> nodes(frameWidth);
    std::vector<stbrp_rect> rects(count);

    // Initialize the packing context with frame dimensions
    stbrp_init_target(&context, frameWidth, frameHeight, nodes.data(), static_cast<int>(nodes.size()));

    // Setup heuristic (optional, but recommended)
    stbrp_setup_heuristic(&context, STBRP_HEURISTIC_Skyline_default);

    // Prepare rectangles for unpacked images
    int rectCount = 0;
    for (size_t i = startIndex; i < count; i++) {
        if (!images[i].packed) {
            rects[rectCount].id = static_cast<int>(i);
            rects[rectCount].w = images[i].width;
            rects[rectCount].h = images[i].height;
            rects[rectCount].was_packed = 0;
            rectCount++;
        }
    }

    // This is the actual stb_rect_pack function call
    stbrp_pack_rects(&context, rects.data(), rectCount);

    // Process results
    for (int i = 0; i < rectCount; i++) {
        if (const auto &rect = rects[i]; rect.was_packed) {
            const int imgIndex = rect.id;
            images[imgIndex].packed = 1;
            images[imgIndex].frameId = frameId;
            images[imgIndex].x = rect.x;
            images[imgIndex].y = rect.y;
        }
    }

    return static_cast<int>(std::ranges::count_if(std::as_const(rects), [](const auto &rect) -> bool { return rect.was_packed; }));
}

// Find optimal frame dimensions given constraint W*H <= S
auto findFrameDimensions(const uint64_t maxArea, const std::vector<const ImageInfo *> &unpacked) -> std::tuple<int, int>
{
    uint64_t w = 0, h = 0;

    // Find max dimensions needed among unpacked images
    uint64_t maxWidth = 0, maxHeight = 0;
    for (const auto &up : unpacked) {
        if (const auto uw = static_cast<uint64_t>(up->width); uw > maxWidth) {
            maxWidth = uw;
        }
        if (const auto uh = static_cast<uint64_t>(up->height); uh > maxHeight) {
            maxHeight = uh;
        }
    }

    // Try to use square-ish dimensions close to sqrt(max_area)
    const auto target = static_cast<uint64_t>(std::sqrt(static_cast<double>(maxArea)));
    assert(target > 0);

    // Ensure we can fit the largest image
    w = target > maxWidth ? target : maxWidth;
    h = maxArea / w;

    // Adjust if height is too small for tallest image
    if (h < maxHeight) {
        h = maxHeight;
        w = maxArea / h;
    }

    // Ensure W*H <= S
    while (w * h > maxArea) {
        if (w > h) {
            w--;
        } else {
            h--;
        }
    }

    assert(w > 0);
    assert(h > 0);

    return {static_cast<int>(w), static_cast<int>(h)};
}

auto packImagesMultiFrame(std::vector<ImageInfo> &images, const uint64_t maxArea, std::vector<Frame> &frames) -> int
{
    assert(maxArea > 0);

    // Sort images by area (largest first)
    std::ranges::sort(images, [](const auto &a, const auto &b) -> bool { return a.width * a.height > b.width * b.height; });

    const auto count = images.size();

    // Initialize packing state
    for (auto &img : images) {
        img.packed = 0;
        img.frameId = -1;
    }

    int frameId = 0;
    size_t totalPacked = 0;

    while (totalPacked < count) {
        // Count unpacked images
        std::vector<const ImageInfo *> unpacked{};
        for (const auto &img : images) {
            if (!img.packed) {
                unpacked.push_back(&img);
            }
        }

        if (unpacked.empty()) {
            break;
        }

        // Find good dimensions for next frame
        auto [frameWidth, frameHeight] = findFrameDimensions(maxArea, unpacked);
        std::cout << "Found FS: (" << frameWidth << ", " << frameHeight << ")\n";

        // Pack into this frame
        int packed = tryPackFrame(images, 0, frameWidth, frameHeight, frameId);
        if (!packed) {
            // Couldn't pack anything, try with smaller dimensions
            frameWidth = frameWidth * 3 / 4;
            frameHeight = static_cast<int>(maxArea) / frameWidth;

            packed = tryPackFrame(images, 0, frameWidth, frameHeight, frameId);
            if (packed == 0) {
                break; // Can't pack remaining images
            }
        }

        // Calculate the actual frame size.
        int actualWidth = 0, actualHeight = 0;
        for (const auto &img : images) {
            if (img.frameId == frameId) {
                actualWidth = std::max(actualWidth, img.x + img.width);
                actualHeight = std::max(actualHeight, img.y + img.height);
            }
        }

        // Record frame
        frames.push_back({.w = actualWidth, .h = actualHeight, .imagesCount = packed});
        frameId++;
        totalPacked += packed;
    }

    return static_cast<int>(totalPacked);
}
