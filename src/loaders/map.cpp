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
#include <utility>

#include "src/algorithms.h"
#include "src/config.h"
#include "src/graphics/engine.h"
#include "src/graphics/resources.h"
#include "src/loaders/json.h"
#include "src/loaders/packing.h"
#include "src/threadpool.h"
#include "src/world/scene.h"

namespace fs = std::filesystem;
namespace algo = algorithms;


namespace Loaders
{

Map::Map(std::string path)
    : m_path(std::move(path))
{
    stbi_set_flip_vertically_on_load(true);
}

inline void copyValues2(decltype(std::views::concat(std::declval<JsonMap &>().movings, std::declval<JsonMap &>().chunks | std::views::join)) json,
                        const JsonMap &map,
                        const std::shared_ptr<World::Scene> &scene)
{
    const auto entitiesCount = scene->entities.size();

    auto pSetupRange = scene->entities.range<Entity::PhysicsSetup>();
    auto pConstraintsRange = scene->entities.range<Entity::PhysicsConstraints>();
    auto pCStateRange = scene->entities.range<Entity::PhysicsCartesianState>();
    auto pAStateRange = scene->entities.range<Entity::PhysicsAngularState>();
    auto pBoundsRange = scene->entities.range<Entity::PhysicsBounds>();
    auto pBBoxRange = scene->entities.range<Entity::AABB>();
    //auto pCStateRange = scene->entities.range<Entity::PhysicsCartesianState>();

    for (const auto &[entity, element] : std::views::zip(pSetupRange, json)) {
        std::get<0>(entity).elasticity = map.resources[element.type].elasticity;
    }

    for (const auto &[entity, element] : std::views::zip(pSetupRange, json)) {
        std::get<0>(entity).mass = map.resources[element.type].mass;
    }

    for (const auto &[entity, element] : std::views::zip(pSetupRange, json)) {
        std::get<0>(entity).canCollide = element.canCollide;
    }

    for (const auto &[entity, element] : std::views::zip(pSetupRange, json)) {
        std::get<0>(entity).isNotFixed = element.isNotFixed;
    }

    for (const auto &[entity, element] : std::views::zip(pConstraintsRange, json)) {
        std::get<0>(entity).friction = element.friction;
    }

    for (const auto &[entity, element] : std::views::zip(pConstraintsRange, json)) {
        std::get<0>(entity).MoI = element.MoI;
    }

    for (const auto &[entity, element] : std::views::zip(pCStateRange, json)) {
        std::get<0>(entity).position = glm::vec2{element.position[0], element.position[1]};
    }

    for (const auto &[entity, element] : std::views::zip(pCStateRange, json)) {
        std::get<0>(entity).velocity = glm::vec2{element.velocity[0], element.velocity[1]};
    }

    for (const auto &[entity, element] : std::views::zip(pCStateRange, json)) {
        std::get<0>(entity).acceleration = glm::vec2{element.acceleration[0], element.acceleration[1]};
    }

    for (const auto &[entity, element] : std::views::zip(pAStateRange, json)) {
        std::get<0>(entity).angularVelocity = element.angularVelocity;
    }

    for (const auto &[entity, element] : std::views::zip(pBoundsRange, json)) {
        std::get<0>(entity).borders = scene->resources->borders[element.type];
    }

    for (const auto &[entity, element] : std::views::zip(pBoundsRange, json)) {
        std::get<0>(entity).normals = scene->resources->normals[element.type];
    }

    for (const auto &[entity, element] : std::views::zip(pBBoxRange, json)) {
        const auto &bb = scene->resources->boundingBoxes[element.type];

        std::get<0>(entity) = Entity::AABB{
            .min = std::get<0>(bb),
            .max = std::get<1>(bb),
        };
    }

    for (const auto &[obj, element] : std::views::zip(scene->objects, json)) {
        obj.verticesId = element.type;
    }
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
    std::vector<ImageInfo> infos{};
    std::unordered_map<std::string, std::tuple<std::vector<glm::vec2>, std::vector<glm::vec2>, std::tuple<glm::vec2, glm::vec2>>> mapped{};
    const uint64_t maxSize = engine->getDeviceMaxImageSize() / 4; // Because we need 4 channels, and each compo on 1 byte.

    infos.resize(imagesMap.size());

    /* Source images loading. */ {
        // Fill infos indexed by the image id assigned in imagesMap (entry.second)

        std::vector<std::future<void>> futures{};
        futures.reserve(imagesMap.size());

        for (const auto &[fst, snd] : imagesMap) {
            const int srcId = snd;
            const std::string path = assetsDir + fst;

            futures.push_back(ThreadPool::instance().enqueue([path, srcId, maxSize, &infos]() -> void {
                ImageInfo &inf = infos[srcId];
                inf = {.frameId = 0, .x = 0, .y = 0, .id = srcId};

                // This gives an 8-bit per channel.
                int channels = 0;
                inf.imgData = stbi_load(path.c_str(), &inf.width, &inf.height, &channels, 4);

                if (!inf.imgData || inf.width <= 0 || inf.height <= 0 || std::cmp_less_equal(maxSize, inf.width * inf.height)) {
                    if (inf.imgData) {
                        stbi_image_free(inf.imgData);
                    }

                    throw std::runtime_error("Load failed: " + path); // Or custom error
                }
            }));
        }

        // Wait & check results
        for (auto &future : futures) {
            try {
                future.get();
            } catch (const std::exception &e) {
                // Cleanup on error
                for (auto &info : infos) {
                    if (info.imgData) {
                        stbi_image_free(info.imgData);
                    }
                }

                return {Status::OpenError, e.what()};
            }
        }
    }

    /* Perform operations related on image data first. */ {
        algo::ImageVectorizer vectorizer{};

        for (const auto &[fst, snd] : imagesMap) {
            const auto &key = fst;
            const auto idx = snd;
            const auto &imgInfo = infos[static_cast<size_t>(idx)];

            // Because each pixel is the @var channels values, mult width by @var channels.
            vectorizer.determineImageBorders(algo::MatrixView(imgInfo.imgData,
                                                              static_cast<size_t>(imgInfo.width * 4),
                                                              static_cast<size_t>(imgInfo.height)),
                                             4); // Because we have RGBA channels.

            mapped.insert({key, {vectorizer.getPoints(), vectorizer.getNormals(), {std::tuple{vectorizer.getMin(), vectorizer.getMax()}}}});
        }
    }

    std::vector<Frame> imageFrames{};
    const int packedCount = packImagesMultiFrame(infos, maxSize, imageFrames);
    assert(packedCount == infos.size());

    // The packing routine may reorder infos; restore original image-id order so indices stay consistent.
    std::ranges::sort(infos, [](const ImageInfo &a, const ImageInfo &b) -> bool { return a.id < b.id; });

    std::vector<std::vector<stbi_uc>> frameImages;
    frameImages.reserve(imageFrames.size());

    /* Allocate grouped images before-hand */
    for (const auto &frame : imageFrames) {
        frameImages.emplace_back(static_cast<size_t>(frame.h) * static_cast<size_t>(frame.w) * 4);
    }

    /* Set up mapping */
    resources->groupedImagesMapping.reserve(infos.size());
    for (const auto &info : infos) {
        resources->groupedImagesMapping.insert({info.id, static_cast<uint32_t>(info.frameId)});
    }

    /* Copy data to its right place */
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

    /* Update animations' data */
    std::cout << "Updating animations...\n";
    for (auto &anim : resources->animations) {
        const auto &info = infos[anim.imageId];
        const auto &frame = imageFrames[info.frameId];

        anim.imageInfo = glm::vec4{
            static_cast<double>(info.x) / static_cast<double>(frame.w),
            static_cast<double>(info.y) / static_cast<double>(frame.h),
            static_cast<double>(info.width) / static_cast<double>(frame.w),
            static_cast<double>(info.height) / static_cast<double>(frame.h),
        };
    }

    for (const auto &res : map.resources) {
        const auto &h = res.h, &w = res.w;

        addVertices(w, h, resources);

        auto points = std::get<0>(mapped[res.source]);
        auto AB = std::get<2>(mapped[res.source]);

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

        resources->borders.push_back(points);
        resources->normals.push_back(scaledNormals);
    }

    for (const auto &info : infos) {
        stbi_image_free(info.imgData);
    }

    return {Status::Ok, ""};
}

void chunkObjectsGrouping(const std::shared_ptr<World::Scene> &scene)
{
    const auto groups = groupBy<Graphics::ObjectData, &Graphics::ObjectData::animationId>(scene->objects);
    scene->references.reserve(groups.size());

    size_t index = 0;
    for (auto group : groups) {
        const size_t start = index;
        const size_t count = std::ranges::distance(group);
        const size_t end = start + count;

        // Safe because the underlying container is contiguous (std::vector) and already sorted.
        scene->references.emplace_back(&scene->objects[start], count);

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
    if (const auto status = loadResources(paths, names, map); std::get<0>(status) != Status::Ok) {
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

    auto flattenedChunks = std::views::concat(map.movings, map.chunks | std::views::join);

    const auto entitiesCount = std::accumulate(map.chunks.cbegin(), map.chunks.cend(), 0, [](const size_t prev, const auto &chunk) -> size_t {
        return prev + chunk.size();
    });

    // We fill it in later to avoid constructing and then change the data.
    scene->entities.resize(entitiesCount);
    scene->objects.resize(entitiesCount);

    /* Set object IDs */ {
        uint64_t currId = 0;
        const auto fn = [&currId](auto &obj) -> void { obj.objId = currId++; };
        std::ranges::for_each(scene->objects, fn);
    }

    // Set object positions
    for (const auto &[chunkElement, obj] : std::views::zip(flattenedChunks, scene->objects)) {
        const auto pos = chunkElement.position;
        obj.position = glm::vec4(pos[0], pos[1], pos[2], 1.f);
    }

    // Set object animation IDs
    for (const auto &[chunkElement, obj] : std::views::zip(flattenedChunks, scene->objects)) {
        // type is the resource index in map.resources; animations were created in
        // the same order, so use type directly as animationId.
        obj.animationId = static_cast<uint32_t>(chunkElement.type);
    }

    // Set object transforms
    std::ranges::for_each(scene->objects, [](auto &obj) -> void { obj.transform = glm::mat4{1.f}; });

    // Update entities' information.
    copyValues2(flattenedChunks, map, scene);

    /* Unique entity ID, different from object ID. */ {
        size_t i = 0;
        const auto objStateRange = scene->entities.range<Entity::PhysicsObjectState>();
        std::ranges::for_each(objStateRange.begin(), objStateRange.end(), [&i](const auto &entity) -> auto { std::get<0>(entity).id = i++; });
    }

    scene->resources->build(engine);

    /* Sort elements by their data and build the views */ {
        std::ranges::sort(scene->objects,
                          [](const Graphics::ObjectData &a, const Graphics::ObjectData &b) -> bool { return a.animationId < b.animationId; });

        chunkObjectsGrouping(scene);
    }

    return {Status::Ok, ""};
}
}
