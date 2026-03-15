#include "map.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

#include <glaze/glaze.hpp>

#include <magic_enum.hpp>

#include <pack2/pack2.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "src/graphics/engine.h"
#include "src/graphics/resources.h"

#include "src/world/scene.h"

#include "src/algorithms.h"

#include "json.h"
#include "packing.h"

namespace fs = std::filesystem;
namespace algo = algorithms;


namespace Loaders
{

Map::Map(std::string path)
    : m_path(std::move(path))
{
    stbi_set_flip_vertically_on_load(true);
}

inline void copyValues2(const std::vector<JsonChunkElement> &c, Graphics::Chunk2 &s, const JsonMap &map, const World::Scene &m_scene)
{
    const auto esize = s.entities.size();
    for (size_t j = 0; j < esize; j++) {
        const auto &e = c[j];

        s.entities[j].position = glm::vec2{e.position[0], e.position[1]};
    }

    for (size_t j = 0; j < esize; j++) {
        const auto &e = c[j];

        s.entities[j].velocity = glm::vec2{e.velocity[0], e.velocity[1]};
    }

    for (size_t j = 0; j < esize; j++) {
        const auto &e = c[j];

        s.entities[j].acceleration = glm::vec2{e.acceleration[0], e.acceleration[1]};
    }

    for (size_t j = 0; j < esize; j++) {
        const auto &e = c[j];

        s.entities[j].friction = e.friction;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].angularVelocity = c[j].angularVelocity;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].canCollide = c[j].canCollide;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].isNotFixed = c[j].isNotFixed;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].MoI = c[j].MoI;
    }

    for (size_t j = 0; j < esize; j++) {
        s.objects[j].verticesId = c[j].type;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].mass = map.resources[c[j].type].mass;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].elasticity = map.resources[c[j].type].elasticity;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].borders = m_scene.res2->borders[c[j].type];
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].normals = m_scene.res2->normals[c[j].type];
    }

    for (size_t j = 0; j < esize; j++) {
        const auto &bb = m_scene.res2->boundingBoxes[c[j].type];

        s.entities[j].boundingBox = Physics::AABB{
            .min = std::get<0>(bb),
            .max = std::get<1>(bb),
        };
    }
}

inline void prepareVectors2(const size_t count, Graphics::Chunk2 &s)
{
    s.entities.resize(count);
}

auto loadChunk(std::vector<std::vector<JsonChunkElement>> &chunks, const std::string &chunkName) -> std::tuple<Status, std::string>
{
    std::ifstream chunkFile(chunkName, std::ifstream::in);
    if (!chunkFile.is_open()) {
        return {Status::OpenError, std::string(__func__) + " " + chunkName};
    }

    const auto chunkResSize = fs::file_size(chunkName);
    std::string chunkContent(chunkResSize, '\0');
    chunkFile.read(chunkContent.data(), static_cast<std::streamsize>(chunkResSize));

    const auto chunkJson = glz::read_json<std::vector<JsonChunkElement>>(chunkContent);
    if (!chunkJson.has_value()) {
        std::cout << "Failed to parse JSON for file " << chunkName << ": " << chunkJson.error().includer_error << '\n';
        return {Status::JsonError, std::string(chunkJson.error().includer_error)};
    }

    chunks.push_back(chunkJson.value());

    return {Status::Ok, ""};
}

auto readMapFile(const std::vector<fs::path> &paths, const std::vector<std::string> &names, JsonMap &out) -> std::tuple<Status, std::string>
{
    const auto mapAccess = std::ranges::find(names, std::string("map.json"));
    if (mapAccess == names.cend()) {
        return {Status::MissingMapFile, "map.json"};
    }

    const auto pathsMapIndex = std::distance(names.cbegin(), mapAccess);
    std::ifstream f(paths[pathsMapIndex], std::ifstream::in);
    if (!f.is_open()) {
        return {Status::OpenError, std::string(__func__) + " " + paths[pathsMapIndex].string()};
    }

    const auto size = fs::file_size(paths[pathsMapIndex]);
    std::string mapContent(size, '\0');
    f.read(mapContent.data(), static_cast<std::streamsize>(size));

    const auto mapJson = glz::read_json<JsonMap>(mapContent);
    if (!mapJson.has_value()) {
        std::cout << "Failed to open file " << paths[pathsMapIndex] << ':' << mapJson.error().location << ':'
                  << magic_enum::enum_name(mapJson.error().ec) << mapJson.error().includer_error << '\n';
        return {Status::JsonError, std::string(mapJson.error().includer_error)};
    }

    out = mapJson.value();

    return {Status::Ok, ""};
}

auto loadResources(const std::vector<fs::path> &paths, const std::vector<std::string> &names, JsonMap &map) -> std::tuple<Status, std::string>
{
    // Load separate resources if relevant.
    if (map.resourcesExternal) {
        const auto resAccess = std::ranges::find(names, std::string("resources.json"));
        if (resAccess == names.cend()) {
            return {Status::MissingJson, "resources.json"};
        }

        const auto pathsResIndex = std::distance(names.cbegin(), resAccess);
        std::ifstream resFile(paths[pathsResIndex], std::ifstream::in);
        if (!resFile.is_open()) {
            return {Status::OpenError, std::string(__func__) + " " + paths[pathsResIndex].string()};
        }

        const auto resSize = std::filesystem::file_size(paths[pathsResIndex]);
        std::string resContent(resSize, '\0');
        resFile.read(resContent.data(), static_cast<std::streamsize>(resSize));

        const auto resJson = glz::read_json<std::vector<JsonResourceElement>>(resContent);
        if (!resJson.has_value()) {
            std::cout << __LINE__;
            std::cout << "Failed to open file " << paths[pathsResIndex] << ':' << resJson.error().location << ':'
                      << magic_enum::enum_name(resJson.error().ec) << ':' << resJson.error().includer_error << ':'
                      << resJson.error().custom_error_message << '\n';
            return {Status::JsonError, std::string(resJson.error().includer_error)};
            ;
        }

        const std::vector<JsonResourceElement> &res = resJson.value();
        map.resources = res; //.resources;
    }

    return {Status::Ok, ""};
}

auto loadChunks(const std::string &m_path, JsonMap &map) -> std::tuple<Status, std::string>
{
    // Load separate chunks if relevant.
    if (map.chunksExternal) {
        const auto chunksSize = map.chunksCount;

        // Check that all files exist
        for (size_t i = 0; i < chunksSize; ++i) {
            const auto fp = m_path + '/' + std::to_string(i) + ".json";
            if (!fs::exists(fp)) {
                return {Status::MissingJson, fp};
            }
        }

        map.chunks.reserve(chunksSize);

        // Try to open & load all chunk files.
        auto status = std::tuple<Status, std::string>{Status::Ok, ""};
        for (size_t i = 0; i < chunksSize && std::get<0>(status) == Status::Ok; ++i) {
            status = loadChunk(map.chunks, m_path + '/' + std::to_string(i) + ".json");
        }

        if (std::get<0>(status) != Status::Ok) {
            return status;
        }

        std::vector<std::vector<JsonChunkElement>> tmp{};
        tmp.reserve(1);

        const auto mvStatus = loadChunk(tmp, m_path + "/movings.json");
        if (std::get<0>(mvStatus) != Status::Ok) {
            return mvStatus;
        }

        map.movings = tmp[0];
    }

    return {Status::Ok, ""};
}

void addVertices(const auto w, const auto h, Graphics::Resources2 &res2)
{
    // Solely for drawing purposes
    const std::array<Graphics::Vertex, 4> vertices = {{
        {
            .position = {0.f, 0.f, 0.f},
            .uv = {0.f, 0.f},
            .normal = {0.f, 0.f, 1.f},
        },
        {
            .position = {0.f, h, 0.f},
            .uv = {0.f, 1.f},
            .normal = {0.f, 0.f, 1.f},
        },
        {
            .position = {w, 0.f, 0.f},
            .uv = {1.f, 0.f},
            .normal = {0.f, 0.f, 1.f},
        },
        {
            .position = {w, h, 0.f},
            .uv = {1.f, 1.f},
            .normal = {0.f, 0.f, 1.f},
        },
    }};

    res2.vertices.append_range(vertices);

    std::ranges::for_each(res2.vertices.cbegin(), res2.vertices.cend(), [](const auto &v) -> void {
        assert(v.uv.x <= 1.f);
        assert(v.uv.y <= 1.f);
    });
}

// Because my clang impl std lib does not provide std::ranges::view for now
template<typename T, auto Member>
auto groupBy(std::vector<T> &v) -> std::vector<std::span<T>>
{
    std::vector<std::span<T>> result{};
    size_t i = 0;

    while (i < v.size()) {
        const size_t start = i;
        const auto current = v[i].*Member;

        while (i < v.size() && v[i].*Member == current) {
            ++i;
        }

        result.emplace_back(&v[start], i - start);
    }

    return result;
}

auto Map::buildResources(const std::unordered_map<std::string, int> &imagesMap,
                         const std::string &m_assets,
                         Graphics::Resources2 &res2,
                         Graphics::Engine &engine,
                         const JsonMap &map) -> std::tuple<Status, std::string>
{
    res2.images.resize(imagesMap.size());

    // Load images and make add relevant data.
    algo::ImageVectorizer vectorizer{};

    std::unordered_map<std::string, std::tuple<std::vector<glm::vec2>, std::vector<glm::vec2>, std::tuple<glm::vec2, glm::vec2>>> mapped{};

    const uint64_t maxSize = engine.getDeviceMaxImageSize() / 4; // Because we need 4 channels, and each compo on 1 byte.
    std::vector<ImageInfo> infos{};
    infos.resize(imagesMap.size());

    /* Source images loading. */ {
        // Fill infos indexed by the image id assigned in imagesMap (entry.second)
        for (const auto &entry : imagesMap) {
            const int srcId = entry.second;
            ImageInfo inf{.frame_id = 0, .x = 0, .y = 0, .id = srcId};

            // This gives an 8-bit per channel.
            int channelUseless;
            inf.imgData = stbi_load((m_assets + entry.first).c_str(), &inf.width, &inf.height, &channelUseless, 4); // Force 4 channels (RGBA)

            if (!inf.imgData) {
                for (auto &pinfo : infos) {
                    if (pinfo.imgData) {
                        stbi_image_free(pinfo.imgData);
                    }
                }

                return {Status::OpenError, std::string(__func__) + " " + (m_assets + entry.first)};
            } else if (maxSize <= static_cast<uint64_t>(inf.width * inf.height) || inf.width <= 0 || inf.height <= 0) {
                if (inf.imgData) stbi_image_free(inf.imgData);
                for (auto &pinfo : infos) {
                    if (pinfo.imgData) stbi_image_free(pinfo.imgData);
                }

                return {Status::MissingRequirement,
                        std::string(__func__) + " " + (m_assets + entry.first) + " " + std::to_string(maxSize) + " vs "
                            + std::to_string(inf.width) + ":" + std::to_string(inf.height)};
            }

            std::cout << "Image [" << inf.id << "]: (" << inf.width << ", " << inf.height << ") at " << (m_assets + entry.first) << "\n";
            infos[static_cast<size_t>(srcId)] = inf;
        }
    }

    /* Perform operations related on image data first. */ {
        for (const auto &entry : imagesMap) {
            const auto &key = entry.first;
            const auto idx = entry.second;
            const auto &imgInfo = infos[static_cast<size_t>(idx)];

            // Because each pixel is the @var channels values, mult width by @var channels.
            vectorizer.determineImageBorders(algo::MatrixView(imgInfo.imgData,
                                                              static_cast<size_t>(imgInfo.width * 4),
                                                              static_cast<size_t>(imgInfo.height)),
                                             4); // Because we have RGBA channels.

            std::cout << "Image [" << imgInfo.id << "]: (" << vectorizer.getMin().x << ", " << vectorizer.getMin().y << ")|(" << vectorizer.getMax().x
                      << ", " << vectorizer.getMax().y << ")\n";
            mapped.insert({key, {vectorizer.getPoints(), vectorizer.getNormals(), {std::tuple{vectorizer.getMin(), vectorizer.getMax()}}}});
        }
    }

    std::vector<Frame> imageFrames{};
    const int packedCount = packImagesMultiFrame(infos, static_cast<uint64_t>(maxSize), imageFrames);
    assert(packedCount == infos.size());

    // The packing routine may reorder infos; restore original image-id order so indices stay consistent.
    std::ranges::sort(infos, [](const ImageInfo &a, const ImageInfo &b) -> bool { return a.id < b.id; });

    {
        int i = 0;
        for (const auto &f : imageFrames) {
            std::cout << "Frame: (" << f.w << ", " << f.h << ", " << f.num_images << ")\n";

            for (const auto &info : infos) {
                if (info.frame_id == i) {
                    std::cout << "(" << info.x << ", " << info.y << ", " << info.width << ", " << info.height << ")\n";
                }
            }

            std::cout << std::flush;
            i++;
        }
    }

    std::vector<std::vector<stbi_uc>> frameImages;
    frameImages.reserve(imageFrames.size());

    /* Allocate grouped images before-hand */ {
        for (const auto &frame : imageFrames) {
            frameImages.emplace_back(static_cast<size_t>(frame.h) * static_cast<size_t>(frame.w) * 4);
        }
    }

    /* Set up mapping */ {
        res2.groupedImagesMapping.reserve(infos.size());
        for (const auto &info : infos) {
            std::cout << "Image [" << info.id << "]: (" << info.width << ", " << info.height << ")\n";
            res2.groupedImagesMapping.insert({info.id, static_cast<uint32_t>(info.frame_id)});
        }
    }

    /* Copy data to its right place */ {
        for (const auto &info : infos) {
            const auto frameInfo = imageFrames[info.frame_id];
            auto &frame = frameImages[info.frame_id];

            const auto src_w = static_cast<size_t>(info.width);
            const auto src_h = static_cast<size_t>(info.height);
            const auto src_row_bytes = src_w * 4;

            const auto frame_w = static_cast<size_t>(frameInfo.w);
            const auto frame_h = static_cast<size_t>(frameInfo.h);

            const size_t frame_total_bytes = frame_w * frame_h * 4;
            const size_t src_total_bytes = src_w * src_h * 4;

            for (size_t row = 0; row < src_h; ++row) {
                const auto dest_row = static_cast<size_t>(info.y) + row;
                const auto dest_col = static_cast<size_t>(info.x);

                const size_t dest_offset = (dest_row * frame_w + dest_col) * 4;
                const size_t src_offset = row * src_row_bytes;

                assert(dest_row < frame_h);
                assert(dest_col + src_w <= frame_w);
                assert(dest_offset + src_row_bytes <= frame_total_bytes);
                assert(src_offset + src_row_bytes <= src_total_bytes);

                std::memcpy(&frame.data()[dest_offset], &info.imgData[src_offset], src_row_bytes);
            }
        }
    }

    /* Create Vulkan images for each packed frame (atlas) */
    res2.images.resize(imageFrames.size());
    for (size_t fi = 0; fi < imageFrames.size(); ++fi) {
        const auto &frameInfo = imageFrames[fi];
        // createImage expects a pointer to pixel data arranged as RGBA
        res2.images[fi].image = engine.createImage(frameImages[fi].data(),
                                                   VkExtent3D{static_cast<uint32_t>(frameInfo.w), static_cast<uint32_t>(frameInfo.h), 1},
                                                   VK_FORMAT_R8G8B8A8_UNORM,
                                                   VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    /* Update animations' data */ {
        std::cout << "Updating animations...\n";
        for (size_t ai = 0; ai < res2.animations.size(); ++ai) {
            auto &anim = res2.animations[ai];
            const auto &info = infos[anim.imageId];
            const auto &frame = imageFrames[info.frame_id];

            anim.imageInfo = glm::vec4{
                static_cast<double>(info.x) / static_cast<double>(frame.w),
                static_cast<double>(info.y) / static_cast<double>(frame.h),
                static_cast<double>(info.width) / static_cast<double>(frame.w),
                static_cast<double>(info.height) / static_cast<double>(frame.h),
            };

            std::cout << "Animation index " << ai << ": imageId(source)=" << anim.imageId
                      << ", mappedFrame=" << info.frame_id
                      << ", grid=" << anim.gridRows << "x" << anim.gridColumns
                      << ", frames=" << anim.framesCount << ", interval=" << anim.frameInterval
                      << ", imageInfo=(" << anim.imageInfo.x << ", " << anim.imageInfo.y << ", " << anim.imageInfo.z << ", " << anim.imageInfo.w << ")\n";
        }
    }

    for (const auto &res : map.resources) {
        const auto &h = res.h, &w = res.w;

        addVertices(w, h, res2);

        auto points = std::get<0>(mapped[res.source]);
        auto AB = std::get<2>(mapped[res.source]);

        std::cout << "Source BB: " << res.source << '\n';
        for (const auto &p : points) {
            std::cout << '(' << p.x << ", " << p.y << ") ";
        }
        std::cout << '\n';

        for (auto &p : points) {
            p.x *= w;
        }
        for (auto &p : points) {
            p.y *= h;
        }

        std::get<0>(AB).x *= w;
        std::get<1>(AB).x *= w;
        std::get<0>(AB).y *= h;
        std::get<1>(AB).y *= h;

        std::cout << "\n======================\n";
        std::cout << "W: " << w << ", H: " << h << '\n';
        std::cout << "Resource with bounding box: (" << std::get<0>(AB).x << ", " << std::get<0>(AB).y << ")  (" << std::get<1>(AB).x << ", "
                  << std::get<1>(AB).y << ")\n";

        for (const auto &point : points) {
            std::cout << '(' << point.x << ", " << point.y << ") ";
        }
        std::cout << '\n';

        res2.boundingBoxes.push_back(AB);
        res2.types.push_back(res.type);
        // recompute normals in the scaled coordinate system
        std::vector<glm::vec2> scaledNormals;
        if (points.size() >= 2) {
            // If points include a closing duplicate at the end, treat edges accordingly
            const size_t distinct = (points.front() == points.back() && points.size() > 1) ? points.size() - 2 : points.size() - 1;

            assert(distinct < points.size());

            scaledNormals.reserve(distinct);
            for (size_t i = 0; i < distinct; ++i) {
                const size_t next = i + 1;
                const glm::vec2 edge = points[next] - points[i];
                const float len2 = glm::dot(edge, edge);
                if (len2 > 1e-12f) {
                    scaledNormals.push_back(glm::normalize(glm::vec2{-edge.y, edge.x}));
                } else if (!scaledNormals.empty()) {
                    scaledNormals.push_back(scaledNormals.back());
                } else {
                    scaledNormals.emplace_back(1.f, 0.f);
                }
            }
        }

        std::cout << "Result BB: " << res.source << '\n';
        for (const auto &p : points) {
            std::cout << '(' << p.x << ", " << p.y << ") ";
        }
        std::cout << '\n';

        res2.borders.push_back(points);
        res2.normals.push_back(scaledNormals);
    }

    for (const auto &info : infos) {
        stbi_image_free(info.imgData);
    }

    return {Status::Ok, ""};
}

void chunkObjectsGrouping(Graphics::Chunk2 &chunk)
{
    const auto groups = groupBy<Graphics::ObjectData, &Graphics::ObjectData::animationId>(chunk.objects);
    chunk.references.reserve(groups.size());

    size_t index = 0;
    for (auto group : groups) {
        const size_t start = index;
        const size_t count = std::ranges::distance(group);
        const size_t end = start + count;

        // Safe because the underlying container is contiguous (std::vector) and already sorted.
        chunk.references.emplace_back(&chunk.objects[start], count);

        index = end;
    }
}

auto Map::load2(Graphics::Engine &engine, World::Scene &m_scene) -> std::tuple<Status, std::string>
{
    if (!fs::exists(m_path)) {
        return {Status::MissingDirectory, m_path};
    }

    if (!fs::is_directory(m_path)) {
        return {Status::NotDir, m_path};
    }

    const std::string m_assets = m_path + "/assets/";
    if (!fs::exists(m_assets)) {
        return {Status::MissingDirectory, m_assets};
    }

    if (!fs::is_directory(m_assets)) {
        return {Status::NotDir, m_assets};
    }

    std::vector<fs::path> paths{};
    std::vector<std::string> names{};
    /* Remove useless path prefixes */ {
        const auto crop = m_path.length() + 1;
        for (const auto &entry : fs::directory_iterator(m_path)) {
            const auto &p = entry.path();
            paths.push_back(p);
            names.push_back(p.string().substr(crop));
        }
    }

    JsonMap map;
    {
        const auto status = readMapFile(paths, names, map);
        if (std::get<0>(status) != Status::Ok) {
            return status;
        }
    }

    // Now we may need to load from external JSON source file (Resources)
    // Or load from external sources the chunk files.
    {
        const auto status = loadResources(paths, names, map);
        if (std::get<0>(status) != Status::Ok) {
            return status;
        }
    }

    // Check that every image resource does exist.
    {
        if (!std::ranges::all_of(map.resources, [&m_assets](const JsonResourceElement &res) -> bool { return fs::exists(m_assets + res.source); })) {
            return {Status::MissingResource, m_assets};
        }
    }

    // Contains ID of the images that are used for animations.
    std::unordered_map<std::string, int> imagesMap{};
    std::vector<uint32_t> resourceToImageId(map.resources.size());
    /* Map every resource to the compact image id deterministically. */ {
        int nextImageId = 0;
        for (size_t r = 0; r < map.resources.size(); ++r) {
            const auto &res = map.resources[r];
            const auto [it, inserted] = imagesMap.try_emplace(res.source, nextImageId);
            if (inserted) {
                ++nextImageId;
            }
            resourceToImageId[r] = static_cast<uint32_t>(it->second);
        }
    }

     m_scene.res2 = new Graphics::Resources2{};

     /* Create the animations */ {
         m_scene.res2->animations.reserve(map.resources.size());

         for (size_t idx = 0; idx < map.resources.size(); ++idx) {
             const auto &res = map.resources[idx];
             m_scene.res2->animations.push_back({
                 .imageId = resourceToImageId[idx],
                 .gridRows = static_cast<uint16_t>(std::get<0>(res.gridSize)),
                 .gridColumns = static_cast<uint16_t>(std::get<1>(res.gridSize)),
                 .framesCount = static_cast<uint16_t>(res.frames ? static_cast<float>(res.frames)
                                                                 : std::get<0>(res.gridSize) * std::get<1>(res.gridSize)),
                 .frameInterval = res.interval,
             });
         }
     }

    {
        const auto status = loadChunks(m_path, map);
        if (std::get<0>(status) != Status::Ok) {
            return status;
        }
    }

    // Now that any external resource have been checked or loaded, we can generate the object.
    // First load the resources.

    const auto resSize = map.resources.size();
    m_scene.res2->images.reserve(resSize);
    m_scene.res2->types.reserve(resSize);
    m_scene.res2->borders.reserve(resSize);
    m_scene.res2->normals.reserve(resSize);

    {
        const auto status = buildResources(imagesMap, m_assets, *m_scene.res2, engine, map);
        if (std::get<0>(status) != Status::Ok) {
            return status;
        }
    }

    m_scene.chunks2.resize(map.chunks.size());
    const auto csize = map.chunks.size();

    m_scene.movings2.objects.resize(map.movings.size());
    for (size_t i = 0; i < csize; ++i) {
        m_scene.chunks2[i].objects.resize(map.chunks[i].size());
    }

    /* Set object IDs */ {
        uint64_t currId = 0;
        const auto fn = [&currId](auto &obj) -> void { obj.objId = currId++; };
        std::ranges::for_each(m_scene.movings2.objects, fn);
        std::ranges::for_each(m_scene.chunks2, [&currId, fn](auto &chunk) -> void {
            currId = 0;
            std::ranges::for_each(chunk.objects, fn);
        });
    }

    /* Set object positions */ {
        for (size_t j = 0; j < map.movings.size(); ++j) {
            const auto pos = map.movings[j].position;

            m_scene.movings2.objects[j].position = glm::vec4(pos[0], pos[1], pos[2], 1.f);
        }

        for (size_t i = 0; i < csize; ++i) {
            const auto size = map.chunks[i].size();
            for (size_t j = 0; j < size; ++j) {
                const auto pos = map.chunks[i][j].position;

                m_scene.chunks2[i].objects[j].position = glm::vec4(pos[0], pos[1], pos[2], 1.f);
            }
        }
    }

    /* Set object animation IDs*/ {
        for (size_t j = 0; j < m_scene.movings2.objects.size(); ++j) {
            // map.movings[j].type is an index into map.resources, which corresponds to the
            // index in m_scene.res2->animations that we built earlier. Use that resource index
            // as the animationId so the renderer looks up the right AnimationData.
            m_scene.movings2.objects[j].animationId = static_cast<uint32_t>(map.movings[j].type);
        }

        for (size_t i = 0; i < csize; ++i) {
            const auto size = map.chunks[i].size();
            for (size_t j = 0; j < size; ++j) {
                // type is the resource index in map.resources; animations were created in
                // the same order, so use type directly as animationId.
                const auto &type = map.chunks[i][j].type;
                m_scene.chunks2[i].objects[j].animationId = static_cast<uint32_t>(type);
            }
        }
    }

    /* Set object transforms */ {
        std::ranges::for_each(m_scene.movings2.objects, [](auto &obj) -> void { obj.transform = glm::mat4{1.f}; });
        std::ranges::for_each(m_scene.chunks2, [](auto &chunk) -> void {
            std::ranges::for_each(chunk.objects, [](auto &obj) -> void { obj.transform = glm::mat4{1.f}; });
        });
    }

    // We fill it in later to avoid constructing and then change the data.
    prepareVectors2(map.movings.size(), m_scene.movings2);
    for (size_t i = 0; i < csize; ++i) {
        prepareVectors2(map.chunks[i].size(), m_scene.chunks2[i]);
    }

    // Update entities' information.
    copyValues2(map.movings, m_scene.movings2, map, m_scene);
    for (size_t i = 0; i < csize; ++i) {
        copyValues2(map.chunks[i], m_scene.chunks2[i], map, m_scene);
    }

    /* Unique entity ID, different from object ID. */ {
        size_t i = 0;
        std::ranges::for_each(m_scene.movings2.entities.begin(), m_scene.movings2.entities.end(), [&i](auto &e) -> void { e.id = i++; });
        std::ranges::for_each(m_scene.chunks2.begin(), m_scene.chunks2.end(), [&i](auto &chunk) -> void {
            std::ranges::for_each(chunk.entities.begin(), chunk.entities.end(), [&i](auto &e) -> void { e.id = i++; });
        });
    }

    m_scene.player2 = &m_scene.movings2.objects.front();
    m_scene.res2->build(engine);

    /* Sort elements by their data and build the views */ {
        constexpr auto sort_cmp = [](const auto &a, const auto &b) -> bool { return a.animationId < b.animationId; };

        std::ranges::sort(m_scene.movings2.objects, sort_cmp);
        std::ranges::for_each(m_scene.chunks2, [](auto &chunk) -> void {
            std::ranges::sort(chunk.objects, [](const auto &a, const auto &b) -> bool { return a.animationId < b.animationId; });
        });

        chunkObjectsGrouping(m_scene.movings2);
        std::ranges::for_each(m_scene.chunks2, &chunkObjectsGrouping);
    }

    m_scene.view2 = m_scene.chunks2 | std::ranges::views::drop(0) | std::ranges::views::take(std::min(size_t(2), csize));

    return {Status::Ok, ""};
}
}
