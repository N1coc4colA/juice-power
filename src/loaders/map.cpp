#include "src/loaders/map.h"

#include <glaze/glaze.hpp>

#include <magic_enum.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <pack2/pack2.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

#include "src/algorithms.h"
#include "src/config.h"
#include "src/graphics/engine.h"
#include "src/graphics/resources.h"
#include "src/loaders/json.h"
#include "src/loaders/packing.h"
#include "src/world/scene.h"

namespace fs = std::filesystem;
namespace algo = algorithms;


namespace Loaders
{

Map::Map(const std::string &path)
    : m_path(std::move(path))
{
    stbi_set_flip_vertically_on_load(true);
}

inline void copyValues2(const std::vector<JsonChunkElement> &jsonChunk,
                        Graphics::Chunk &chunk,
                        const JsonMap &map,
                        const std::shared_ptr<World::Scene> &scene)
{
    const auto entitiesCount = chunk.entities.size();
    for (size_t j = 0; j < entitiesCount; j++) {
        const auto &entity = jsonChunk[j];

        chunk.entities[j].position = glm::vec2{entity.position[0], entity.position[1]};
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        const auto &entity = jsonChunk[j];

        chunk.entities[j].velocity = glm::vec2{entity.velocity[0], entity.velocity[1]};
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        const auto &entity = jsonChunk[j];

        chunk.entities[j].acceleration = glm::vec2{entity.acceleration[0], entity.acceleration[1]};
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        const auto &entity = jsonChunk[j];

        chunk.entities[j].friction = entity.friction;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].angularVelocity = jsonChunk[j].angularVelocity;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].canCollide = jsonChunk[j].canCollide;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].isNotFixed = jsonChunk[j].isNotFixed;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].MoI = jsonChunk[j].MoI;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.objects[j].verticesId = jsonChunk[j].type;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].mass = map.resources[jsonChunk[j].type].mass;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].elasticity = map.resources[jsonChunk[j].type].elasticity;
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].borders = scene->resources->borders[jsonChunk[j].type];
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        chunk.entities[j].normals = scene->resources->normals[jsonChunk[j].type];
    }

    for (size_t j = 0; j < entitiesCount; j++) {
        const auto &bb = scene->resources->boundingBoxes[jsonChunk[j].type];

        chunk.entities[j].boundingBox = Physics::AABB{
            .min = std::get<0>(bb),
            .max = std::get<1>(bb),
        };
    }
}

inline void prepareVectors2(const size_t count, Graphics::Chunk &chunk)
{
    chunk.entities.resize(count);
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
        }

        const std::vector<JsonResourceElement> &res = resJson.value();
        map.resources = res; //.resources;
    }

    return {Status::Ok, ""};
}

auto loadChunks(const std::string &directory, JsonMap &map) -> std::tuple<Status, std::string>
{
    // Load separate chunks if relevant.
    if (map.chunksExternal) {
        const auto chunksSize = map.chunksCount;

        // Check that all files exist
        for (size_t i = 0; i < chunksSize; ++i) {
            if (const auto fp = directory + '/' + std::to_string(i) + ".json"; !fs::exists(fp)) {
                return {Status::MissingJson, fp};
            }
        }

        map.chunks.reserve(chunksSize);

        // Try to open & load all chunk files.
        auto status = std::tuple<Status, std::string>{Status::Ok, ""};
        for (size_t i = 0; i < chunksSize && std::get<0>(status) == Status::Ok; ++i) {
            status = loadChunk(map.chunks, directory + '/' + std::to_string(i) + ".json");
        }

        if (std::get<0>(status) != Status::Ok) {
            return status;
        }

        std::vector<std::vector<JsonChunkElement>> tmp{};
        tmp.reserve(1);

        if (const auto mvStatus = loadChunk(tmp, directory + "/movings.json"); std::get<0>(mvStatus) != Status::Ok) {
            return mvStatus;
        }

        map.movings = tmp[0];
    }

    return {Status::Ok, ""};
}

void addVertices(const auto width, const auto height, const std::shared_ptr<Graphics::Resources> &resources)
{
    // Solely for drawing purposes
    const std::array<Graphics::Vertex, 4> vertices = {{
        {
            .position = {0.f, 0.f, 0.f},
            .uv = {0.f, 0.f},
            .normal = {0.f, 0.f, 1.f},
        },
        {
            .position = {0.f, height, 0.f},
            .uv = {0.f, 1.f},
            .normal = {0.f, 0.f, 1.f},
        },
        {
            .position = {width, 0.f, 0.f},
            .uv = {1.f, 0.f},
            .normal = {0.f, 0.f, 1.f},
        },
        {
            .position = {width, height, 0.f},
            .uv = {1.f, 1.f},
            .normal = {0.f, 0.f, 1.f},
        },
    }};

    resources->vertices.append_range(vertices);

    std::ranges::for_each(resources->vertices.cbegin(), resources->vertices.cend(), [](const auto &v) -> void {
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
                         const std::string &assetsDir,
                         const std::shared_ptr<Graphics::Resources> &resources,
                         const std::shared_ptr<Graphics::Engine> &engine,
                         const JsonMap &map) -> std::tuple<Status, std::string>
{
    resources->images.resize(imagesMap.size());

    // Load images and make add relevant data.
    algo::ImageVectorizer vectorizer{};

    std::unordered_map<std::string, std::tuple<std::vector<glm::vec2>, std::vector<glm::vec2>, std::tuple<glm::vec2, glm::vec2>>> mapped{};

    const uint64_t maxSize = engine->getDeviceMaxImageSize() / 4; // Because we need 4 channels, and each compo on 1 byte.
    std::vector<ImageInfo> infos{};
    infos.resize(imagesMap.size());

    /* Source images loading. */ {
        // Fill infos indexed by the image id assigned in imagesMap (entry.second)
        for (const auto & [fst, snd] : imagesMap) {
            const int srcId = snd;
            ImageInfo inf{.frameId = 0, .x = 0, .y = 0, .id = srcId};

            // This gives an 8-bit per channel.
            int channelUseless;
            inf.imgData = stbi_load((assetsDir + fst).c_str(), &inf.width, &inf.height, &channelUseless, 4); // Force 4 channels (RGBA)

            if (!inf.imgData) {
                for (auto &info : infos) {
                    if (info.imgData != nullptr) {
                        stbi_image_free(info.imgData);
                    }
                }

                return {Status::OpenError, std::string(__func__) + " " + (assetsDir + fst)};
            }

            if (maxSize <= static_cast<uint64_t>(inf.width * inf.height) || inf.width <= 0 || inf.height <= 0) {
                stbi_image_free(inf.imgData);

                for (auto &info : infos) {
                    if (info.imgData) {
                        stbi_image_free(info.imgData);
                    }
                }

                return {Status::MissingRequirement,
                        std::string(__func__) + " " + (assetsDir + fst) + " " + std::to_string(maxSize) + " vs " + std::to_string(inf.width) + ":"
                            + std::to_string(inf.height)};
            }

            std::cout << "Image [" << inf.id << "]: (" << inf.width << ", " << inf.height << ") at " << (assetsDir + fst) << "\n";
            infos[static_cast<size_t>(srcId)] = inf;
        }
    }

    /* Perform operations related on image data first. */ {
        for (const auto & [fst, snd] : imagesMap) {
            const auto &key = fst;
            const auto idx = snd;
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
    const int packedCount = packImagesMultiFrame(infos, maxSize, imageFrames);
    assert(packedCount == infos.size());

    // The packing routine may reorder infos; restore original image-id order so indices stay consistent.
    std::ranges::sort(infos, [](const ImageInfo &a, const ImageInfo &b) -> bool { return a.id < b.id; });

    {
        int i = 0;
        for (const auto &[w, h, imagesCount] : imageFrames) {
            std::cout << "Frame: (" << w << ", " << h << ", " << imagesCount << ")\n";

            for (const auto &info : infos) {
                if (info.frameId == i) {
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
        resources->groupedImagesMapping.reserve(infos.size());
        for (const auto &info : infos) {
            std::cout << "Image [" << info.id << "]: (" << info.width << ", " << info.height << ")\n";
            resources->groupedImagesMapping.insert({info.id, static_cast<uint32_t>(info.frameId)});
        }
    }

    /* Copy data to its right place */ {
        for (const auto &info : infos) {
            const auto frameInfo = imageFrames[info.frameId];
            auto &frame = frameImages[info.frameId];

            const auto srcWidth = static_cast<size_t>(info.width);
            const auto srcHeight = static_cast<size_t>(info.height);
            const auto srcRowBytes = srcWidth * 4;

            const auto frameWidth = static_cast<size_t>(frameInfo.w);
            const auto frameHeight = static_cast<size_t>(frameInfo.h);

            const size_t frameTotalBytes = frameWidth * frameHeight * 4;
            const size_t srcTotalytes = srcWidth * srcHeight * 4;

            for (size_t row = 0; row < srcHeight; ++row) {
                const auto destRow = static_cast<size_t>(info.y) + row;
                const auto destCol = static_cast<size_t>(info.x);

                const size_t destOffset = (destRow * frameWidth + destCol) * 4;
                const size_t srcOffset = row * srcRowBytes;

                assert(destRow < frameHeight);
                assert(destCol + srcWidth <= frameWidth);
                assert(destOffset + srcRowBytes <= frameTotalBytes);
                assert(srcOffset + srcRowBytes <= srcTotalytes);

                std::memcpy(&frame.data()[destOffset], &info.imgData[srcOffset], srcRowBytes);
            }
        }
    }

    /* Create Vulkan images for each packed frame (atlas) */
    resources->images.resize(imageFrames.size());
    for (size_t fi = 0; fi < imageFrames.size(); ++fi) {
        const auto &frameInfo = imageFrames[fi];
        // createImage expects a pointer to pixel data arranged as RGBA
        resources->images[fi].image = engine->createImage(frameImages[fi].data(),
                                                          VkExtent3D{static_cast<uint32_t>(frameInfo.w), static_cast<uint32_t>(frameInfo.h), 1},
                                                          VK_FORMAT_R8G8B8A8_UNORM,
                                                          VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    /* Update animations' data */ {
        std::cout << "Updating animations...\n";
        for (size_t ai = 0; ai < resources->animations.size(); ++ai) {
            auto &anim = resources->animations[ai];
            const auto &info = infos[anim.imageId];
            const auto &frame = imageFrames[info.frameId];

            anim.imageInfo = glm::vec4{
                static_cast<double>(info.x) / static_cast<double>(frame.w),
                static_cast<double>(info.y) / static_cast<double>(frame.h),
                static_cast<double>(info.width) / static_cast<double>(frame.w),
                static_cast<double>(info.height) / static_cast<double>(frame.h),
            };

            std::cout << "Animation index " << ai << ": imageId(source)=" << anim.imageId
                      << ", mappedFrame=" << info.frameId
                      << ", grid=" << anim.gridRows << "x" << anim.gridColumns
                      << ", frames=" << anim.framesCount << ", interval=" << anim.frameInterval
                      << ", imageInfo=(" << anim.imageInfo.x << ", " << anim.imageInfo.y << ", " << anim.imageInfo.z << ", " << anim.imageInfo.w << ")\n";
        }
    }

    for (const auto &res : map.resources) {
        const auto &h = res.h, &w = res.w;

        addVertices(w, h, resources);

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

        resources->boundingBoxes.push_back(AB);
        resources->types.push_back(res.type);
        // recompute normals in the scaled coordinate system
        std::vector<glm::vec2> scaledNormals;
        if (points.size() >= 2) {
            // If points include a closing duplicate at the end, treat edges accordingly
            const size_t distinct = points.front() == points.back() && points.size() > 1 ? points.size() - 2 : points.size() - 1;

            assert(distinct < points.size());

            scaledNormals.reserve(distinct);
            for (size_t i = 0; i < distinct; ++i) {
                const size_t next = i + 1;

                if (const glm::vec2 edge = points[next] - points[i]; glm::dot(edge, edge) > Config::potracePointError) {
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

        resources->borders.push_back(points);
        resources->normals.push_back(scaledNormals);
    }

    for (const auto &info : infos) {
        stbi_image_free(info.imgData);
    }

    return {Status::Ok, ""};
}

void chunkObjectsGrouping(Graphics::Chunk &chunk)
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

auto Map::load2(const std::shared_ptr<Graphics::Engine> &engine, const std::shared_ptr<World::Scene> &scene) -> std::tuple<Status, std::string>
{
    if (!fs::exists(m_path)) {
        return {Status::MissingDirectory, m_path};
    }

    if (!fs::is_directory(m_path)) {
        return {Status::NotDir, m_path};
    }

    const std::string assetsDir = m_path + "/assets/";
    if (!fs::exists(assetsDir)) {
        return {Status::MissingDirectory, assetsDir};
    }

    if (!fs::is_directory(assetsDir)) {
        return {Status::NotDir, assetsDir};
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
    if (const auto status = readMapFile(paths, names, map); std::get<0>(status) != Status::Ok) {
        return status;
    }

    // Now we may need to load from external JSON source file (Resources)
    // Or load from external sources the chunk files.
    if (const auto status = loadResources(paths, names, map);std::get<0>(status) != Status::Ok) {
        return status;
    }

    // Check that every image resource does exist.
    if (!std::ranges::all_of(map.resources, [&assetsDir](const JsonResourceElement &res) -> bool { return fs::exists(assetsDir + res.source); })) {
        return {Status::MissingResource, assetsDir};
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

    scene->resources = std::make_shared<Graphics::Resources>();

    /* Create the animations */ {
        scene->resources->animations.reserve(map.resources.size());

        for (size_t idx = 0; idx < map.resources.size(); ++idx) {
            const auto &res = map.resources[idx];
            scene->resources->animations.push_back({
                .imageId = resourceToImageId[idx],
                .gridRows = static_cast<uint16_t>(std::get<0>(res.gridSize)),
                .gridColumns = static_cast<uint16_t>(std::get<1>(res.gridSize)),
                .framesCount = static_cast<uint16_t>(res.frames ? static_cast<float>(res.frames)
                                                                : std::get<0>(res.gridSize) * std::get<1>(res.gridSize)),
                .frameInterval = res.interval,
            });
        }
    }

    if (const auto status = loadChunks(m_path, map); std::get<0>(status) != Status::Ok) {
        return status;
    }

    // Now that any external resource have been checked or loaded, we can generate the object.
    // First load the resources.

    const auto resSize = map.resources.size();
    scene->resources->images.reserve(resSize);
    scene->resources->types.reserve(resSize);
    scene->resources->borders.reserve(resSize);
    scene->resources->normals.reserve(resSize);

    if (const auto status = buildResources(imagesMap, assetsDir, scene->resources, engine, map); std::get<0>(status) != Status::Ok) {
        return status;
    }

    scene->chunks.resize(map.chunks.size());
    const auto chunksCount = map.chunks.size();

    scene->movings.objects.resize(map.movings.size());
    for (size_t i = 0; i < chunksCount; ++i) {
        scene->chunks[i].objects.resize(map.chunks[i].size());
    }

    /* Set object IDs */ {
        uint64_t currId = 0;
        const auto fn = [&currId](auto &obj) -> void { obj.objId = currId++; };
        std::ranges::for_each(scene->movings.objects, fn);
        std::ranges::for_each(scene->chunks, [&currId, fn](auto &chunk) -> void {
            currId = 0;
            std::ranges::for_each(chunk.objects, fn);
        });
    }

    /* Set object positions */ {
        for (size_t j = 0; j < map.movings.size(); ++j) {
            const auto pos = map.movings[j].position;

            scene->movings.objects[j].position = glm::vec4(pos[0], pos[1], pos[2], 1.f);
        }

        for (size_t i = 0; i < chunksCount; ++i) {
            const auto size = map.chunks[i].size();
            for (size_t j = 0; j < size; ++j) {
                const auto pos = map.chunks[i][j].position;

                scene->chunks[i].objects[j].position = glm::vec4(pos[0], pos[1], pos[2], 1.f);
            }
        }
    }

    /* Set object animation IDs*/ {
        for (size_t j = 0; j < scene->movings.objects.size(); ++j) {
            // map.movings[j].type is an index into map.resources, which corresponds to the
            // index in m_scene->res2->animations that we built earlier. Use that resource index
            // as the animationId so the renderer looks up the right AnimationData.
            scene->movings.objects[j].animationId = static_cast<uint32_t>(map.movings[j].type);
        }

        for (size_t i = 0; i < chunksCount; ++i) {
            const auto size = map.chunks[i].size();
            for (size_t j = 0; j < size; ++j) {
                // type is the resource index in map.resources; animations were created in
                // the same order, so use type directly as animationId.
                const auto &type = map.chunks[i][j].type;
                scene->chunks[i].objects[j].animationId = static_cast<uint32_t>(type);
            }
        }
    }

    /* Set object transforms */ {
        std::ranges::for_each(scene->movings.objects, [](auto &obj) -> void { obj.transform = glm::mat4{1.f}; });
        std::ranges::for_each(scene->chunks, [](auto &chunk) -> void {
            std::ranges::for_each(chunk.objects, [](auto &obj) -> void { obj.transform = glm::mat4{1.f}; });
        });
    }

    // We fill it in later to avoid constructing and then change the data.
    prepareVectors2(map.movings.size(), scene->movings);
    for (size_t i = 0; i < chunksCount; ++i) {
        prepareVectors2(map.chunks[i].size(), scene->chunks[i]);
    }

    // Update entities' information.
    copyValues2(map.movings, scene->movings, map, scene);
    for (size_t i = 0; i < chunksCount; ++i) {
        copyValues2(map.chunks[i], scene->chunks[i], map, scene);
    }

    /* Unique entity ID, different from object ID. */ {
        size_t i = 0;
        std::ranges::for_each(scene->movings.entities.begin(), scene->movings.entities.end(), [&i](auto &e) -> void { e.id = i++; });
        std::ranges::for_each(scene->chunks.begin(), scene->chunks.end(), [&i](auto &chunk) -> void {
            std::ranges::for_each(chunk.entities.begin(), chunk.entities.end(), [&i](auto &entity) -> void { entity.id = i++; });
        });
    }

    scene->player2 = &scene->movings.objects.front();
    scene->resources->build(engine);

    /* Sort elements by their data and build the views */ {
        constexpr auto sortCmp = [](const auto &a, const auto &b) -> bool { return a.animationId < b.animationId; };

        std::ranges::sort(scene->movings.objects, sortCmp);
        std::ranges::for_each(scene->chunks, [](auto &chunk) -> void {
            std::ranges::sort(chunk.objects, [](const auto &a, const auto &b) -> bool { return a.animationId < b.animationId; });
        });

        chunkObjectsGrouping(scene->movings);
        std::ranges::for_each(scene->chunks, &chunkObjectsGrouping);
    }

    scene->view = scene->chunks | std::ranges::views::drop(0) | std::ranges::views::take(std::min(static_cast<size_t>(2), chunksCount));

    return {Status::Ok, ""};
}
}
