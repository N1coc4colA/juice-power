#include "packing.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <algorithm>
#include <cmath>
#include <iostream>

// Try packing images into a frame with given dimensions using stb_rect_pack
auto tryPackFrame(std::vector<ImageInfo> &images, const int start_idx, const int frame_w, const int frame_h, const int frame_id) -> int
{
    const auto count = images.size();
    stbrp_context context{};

    // Allocate nodes for the packing algorithm
    std::vector<stbrp_node> nodes(frame_w);
    std::vector<stbrp_rect> rects(count);

    // Initialize the packing context with frame dimensions
    stbrp_init_target(&context, frame_w, frame_h, nodes.data(), static_cast<int>(nodes.size()));

    // Setup heuristic (optional, but recommended)
    stbrp_setup_heuristic(&context, STBRP_HEURISTIC_Skyline_default);

    // Prepare rectangles for unpacked images
    int rect_count = 0;
    for (int i = start_idx; i < count; i++) {
        if (!images[i].packed) {
            rects[rect_count].id = i;
            rects[rect_count].w = images[i].width;
            rects[rect_count].h = images[i].height;
            rects[rect_count].was_packed = 0;
            rect_count++;
        }
    }

    // This is the actual stb_rect_pack function call
    stbrp_pack_rects(&context, rects.data(), rect_count);

    // Process results
    for (int i = 0; i < rect_count; i++) {
        const auto &rect = rects[i];
        if (rect.was_packed) {
            const int img_idx = rect.id;
            images[img_idx].packed = 1;
            images[img_idx].frame_id = frame_id;
            images[img_idx].x = rect.x;
            images[img_idx].y = rect.y;
        }
    }

    return static_cast<int>(std::count_if(rects.cbegin(), rects.cend(), [](const auto &rect) -> bool { return rect.was_packed; }));
}

// Find optimal frame dimensions given constraint W*H <= S
auto findFrameDimensions(const uint64_t maxArea, const std::vector<const ImageInfo *> &unpacked) -> std::tuple<int, int>
{
    uint64_t w = 0, h = 0;

    // Find max dimensions needed among unpacked images
    uint64_t max_w = 0, max_h = 0;
    for (const auto &up : unpacked) {
        const auto uw = static_cast<uint64_t>(up->width), uh = static_cast<uint64_t>(up->height);
        if (uw > max_w) {
            max_w = uw;
        }
        if (uh > max_h) {
            max_h = uh;
        }
    }

    // Try to use square-ish dimensions close to sqrt(max_area)
    const auto target = static_cast<uint64_t>(std::sqrt(static_cast<double>(maxArea)));
    assert(target > 0);

    // Ensure we can fit the largest image
    w = target > max_w ? target : max_w;
    h = maxArea / w;

    // Adjust if height is too small for tallest image
    if (h < max_h) {
        h = max_h;
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
        img.frame_id = -1;
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
        auto [frame_w, frame_h] = findFrameDimensions(maxArea, unpacked);
        std::cout << "Found FS: (" << frame_w << ", " << frame_h << ")\n";

        // Pack into this frame
        int packed = tryPackFrame(images, 0, frame_w, frame_h, frameId);
        if (!packed) {
            // Couldn't pack anything, try with smaller dimensions
            frame_w = frame_w * 3 / 4;
            frame_h = static_cast<int>(maxArea) / frame_w;

            packed = tryPackFrame(images, 0, frame_w, frame_h, frameId);
            if (packed == 0) {
                break; // Can't pack remaining images
            }
        }

        // Calculate the actual frame size.
        int actual_w = 0, actual_h = 0;
        for (const auto &img : images) {
            if (img.frame_id == frameId) {
                actual_w = std::max(actual_w, img.x + img.width);
                actual_h = std::max(actual_h, img.y + img.height);
            }
        }

        // Record frame
        frames.push_back({.w = actual_w, .h = actual_h, .num_images = packed});
        frameId++;
        totalPacked += packed;
    }

    return totalPacked;
}
