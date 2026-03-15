#ifndef PACKING_H
#define PACKING_H

#include <cstdint>
#include <vector>

using stbi_uc = unsigned char;

struct ImageInfo
{
    int width, height;
    int frame_id; // Which frame this image was packed into
    int x, y;     // Position in frame
    int packed;   // Whether successfully packed
    stbi_uc *imgData = nullptr;
    int id;
};

struct Frame
{
    int w, h;
    int num_images;
};

int packImagesMultiFrame(std::vector<ImageInfo> &images, uint64_t max_area, std::vector<Frame> &frames);

#endif // PACKING_H
