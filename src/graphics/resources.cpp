#include "resources.h"

#include <span>

#include "engine.h"


namespace Graphics
{

void Resources2::build(Engine &engine)
{
    const auto size = vertices.size();

    /* Upload edges */ {
        std::vector<uint32_t> indices{};
        indices.reserve((size / 4) * 6);

        // Fill in the indices.
        for (size_t i = 0; i < size; i += 4) {
            indices.push_back(i);
            indices.push_back(i + 1);
            indices.push_back(i + 3);
            indices.push_back(i);
            indices.push_back(i + 3);
            indices.push_back(i + 2);
        }

        meshBuffers = engine.uploadMesh(indices, vertices);
    }

    /* Upload borders */ {
        borderOffsets.reserve(size);

        size_t bordersMaxPoints = 0;
        for (const auto &v : borders) {
            const auto s = v.size();
            if (bordersMaxPoints < s) {
                bordersMaxPoints = s;
            }
        }

        std::vector<LineVertex> gpuBorders(borders.size() * bordersMaxPoints);

        uint32_t i = 0;
        for (const auto &v : borders) {
            borderOffsets.push_back(i);

            std::transform(v.cbegin(), v.cend(), gpuBorders.begin() + i, [](const auto &p) { return LineVertex{p}; });

            assert(!v.empty());

            i += static_cast<uint32_t>(v.size());
        }

        gpuBorders.shrink_to_fit();
        const auto bsize = gpuBorders.size();
        assert(bsize != 0);

        std::vector<uint32_t> indices{};
        indices.reserve(bsize);
        for (uint32_t j = 0; j < bsize; ++j) {
            indices.push_back(j);
        }

        linesBuffer = engine.uploadMesh(indices, gpuBorders);
    }

    animationsBuffer = engine.uploadMesh(animations);
}

void Resources2::cleanup(Engine &engine)
{
    engine.destroyBuffer(meshBuffers.indexBuffer);
    engine.destroyBuffer(meshBuffers.vertexBuffer);
    engine.destroyBuffer(linesBuffer.indexBuffer);
    engine.destroyBuffer(linesBuffer.vertexBuffer);
    engine.destroyBuffer(pointsBuffer.indexBuffer);
    engine.destroyBuffer(pointsBuffer.vertexBuffer);
    engine.destroyBuffer(animationsBuffer.animationBuffer);

    meshBuffers = {};

    for (const auto &img : images) {
        engine.destroyImage(img);
    }

    images.clear();
    vertices.clear();
    types.clear();
    borders.clear();
    normals.clear();
    boundingBoxes.clear();
    borderOffsets.clear();
}
}
