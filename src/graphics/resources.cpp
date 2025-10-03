#include "resources.h"

#include <span>

#include "engine.h"


namespace Graphics
{

void Resources::build(Engine &engine)
{
    const auto size = vertices.size();
    {
        std::vector<uint32_t> indices{};
        indices.reserve(size * 4);

        // Fill in the indices.
        for (size_t i = 0; i < size; i++) {
            const auto v = i * 4;

            indices.push_back(v);
            indices.push_back(v + 1);
            indices.push_back(v + 3);
            indices.push_back(v);
            indices.push_back(v + 3);
            indices.push_back(v + 2);
        }

        {
            // Vertices is an array of Vertex, so it's ok to cast it directly.
            const auto span = std::span<Vertex>(reinterpret_cast<Vertex *>(vertices.data()), size * 4);
            meshBuffers = engine.uploadMesh(indices, span);
        }
    }

    {
        borderOffsets.reserve(size);

        size_t bordersMaxPoints = 0;
        for (const auto &v : borders) {
            const auto s = v.size();
            if (bordersMaxPoints < s) {
                bordersMaxPoints = s;
            }
        }

        std::vector<LineVertex> gpuBorders{};
        gpuBorders.reserve(borders.size() * bordersMaxPoints);

        uint32_t i = 0;
        for (const auto &v : borders) {
            borderOffsets.push_back(i);

            for (const auto &p : v) {
                gpuBorders.push_back(LineVertex{p});
            }

            assert(!v.empty());

            i += static_cast<uint32_t>(v.size());
        }

        gpuBorders.shrink_to_fit();
        const auto bsize = gpuBorders.size();

        std::vector<uint32_t> indices{};
        indices.reserve(bsize);
        for (size_t i = 0; i < bsize; ++i) {
            indices.push_back(i);
        }

        {
            // Vertices is an array of Vertex, so it's ok to cast it directly.
            const auto span = std::span<LineVertex>(reinterpret_cast<LineVertex *>(gpuBorders.data()), bsize);
            linesBuffer = engine.uploadMesh(indices, span);
        }
    }
}

void Resources::cleanup(Engine &engine)
{
    engine.destroyBuffer(meshBuffers.indexBuffer);
    engine.destroyBuffer(meshBuffers.vertexBuffer);

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
