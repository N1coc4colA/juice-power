#include "map.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

#include <glaze/glaze.hpp>

#include <magic_enum.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "src/graphics/engine.h"
#include "src/graphics/resources.h"

#include "src/world/scene.h"

#include "src/algorithms.h"

#include "json.h"


namespace fs = std::filesystem;
namespace algo = algorithms;


namespace Loaders
{

Map::Map(const std::string &path)
	: m_path(path)
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

Status loadChunk(std::vector<std::vector<JsonChunkElement>> &chunks, const std::string &chunkName)
{
    std::ifstream chunkFile(chunkName, std::ifstream::in);
    if (!chunkFile.is_open()) {
        return Status::OpenError;
    }

    const auto chunkResSize = fs::file_size(chunkName);
    std::string chunkContent(chunkResSize, '\0');
    chunkFile.read(chunkContent.data(), static_cast<std::streamsize>(chunkResSize));

    const auto chunkJson = glz::read_json<std::vector<JsonChunkElement>>(chunkContent);
    if (!chunkJson.has_value()) {
        std::cout << __LINE__;
        std::cout << "Failed to open file " << chunkName << ':' << chunkJson.error().location << ':' << magic_enum::enum_name(chunkJson.error().ec)
                  << ':' << chunkJson.error().includer_error << '\n';
        return Status::JsonError;
    }

    chunks.push_back(chunkJson.value());

    return Status::Ok;
}

Status readMapFile(const std::vector<fs::path> &paths, const std::vector<std::string> &names, JsonMap &out)
{
    const auto mapAccess = std::find(names.cbegin(), names.cend(), "map.json");
    if (mapAccess == names.cend()) {
        return Status::MissingMapFile;
    }

    const auto pathsMapIndex = std::distance(names.cbegin(), mapAccess);
    std::ifstream f(paths[pathsMapIndex], std::ifstream::in);
    if (!f.is_open()) {
        return Status::OpenError;
    }

    const auto size = fs::file_size(paths[pathsMapIndex]);
    std::string mapContent(size, '\0');
    f.read(mapContent.data(), static_cast<std::streamsize>(size));

    const auto mapJson = glz::read_json<JsonMap>(mapContent);
    if (!mapJson.has_value()) {
        std::cout << "Failed to open file " << paths[pathsMapIndex] << ':' << mapJson.error().location << ':'
                  << magic_enum::enum_name(mapJson.error().ec) << mapJson.error().includer_error << '\n';
        return Status::JsonError;
    }

    out = mapJson.value();

    return Status::Ok;
}

Status loadResources(const std::vector<fs::path> &paths, const std::vector<std::string> &names, JsonMap &map)
{
    // Load separate resources if relevant.
    if (map.resourcesExternal) {
        const auto resAccess = std::find(names.cbegin(), names.cend(), "resources.json");
        if (resAccess == names.cend()) {
            return Status::MissingJson;
        }

        const auto pathsResIndex = std::distance(names.cbegin(), resAccess);
        std::ifstream resFile(paths[pathsResIndex], std::ifstream::in);
        if (!resFile.is_open()) {
            return Status::OpenError;
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
            return Status::JsonError;
        }

        const std::vector<JsonResourceElement> &res = resJson.value();
        map.resources = res; //.resources;
    }

    return Status::Ok;
}

Status loadChunks(const std::string &m_path, JsonMap &map)
{
    // Load separate chunks if relevant.
    if (map.chunksExternal) {
        const auto chunksSize = map.chunksCount;

        // Check that all files exist
        for (size_t i = 0; i < chunksSize; ++i) {
            if (!fs::exists(m_path + '/' + std::to_string(i) + ".json")) {
                return Status::MissingJson;
            }
        }

        map.chunks.reserve(chunksSize);

        // Try to open & load all chunk files.
        auto status = Status::Ok;
        for (size_t i = 0; i < chunksSize && status == Status::Ok; ++i) {
            status = loadChunk(map.chunks, m_path + '/' + std::to_string(i) + ".json");
        }

        if (status != Status::Ok) {
            return status;
        }

        std::vector<std::vector<JsonChunkElement>> tmp{};
        tmp.reserve(1);
        if (const auto mvStatus = loadChunk(tmp, m_path + "/movings.json")) {
            return mvStatus;
        }

        map.movings = tmp[0];
    }

    return Status::Ok;
}

void addVertices(const auto w, const auto h, Graphics::Resources2 &res2)
{
    // Solely for drawing purposes
    static const Graphics::Vertex vertices[4]{
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
    };

    res2.vertices.append_range(std::span(vertices, 4));

    std::for_each(res2.vertices.cbegin(), res2.vertices.cend(), [](const auto &v) {
        assert(v.uv.x <= 1.f);
        assert(v.uv.y <= 1.f);
    });
}

// Because my clang impl std lib does not provide std::ranges::view for now
template<typename T, auto Member>
std::vector<std::span<T>> group_by(std::vector<T> &v)
{
    std::vector<std::span<T>> result{};
    size_t i = 0;

    while (i < v.size()) {
        const size_t start = i;
        const auto current = v[i].*Member;

        while (i < v.size() && v[i].*Member == current) {
            ++i;
        }

        result.emplace_back(v.data() + start, i - start);
    }

    return result;
}

Status Map::buildResources(const std::unordered_map<std::string, int> &imagesMap,
                           const std::string &m_assets,
                           Graphics::Resources2 &res2,
                           Graphics::Engine &engine,
                           const JsonMap &map)
{
    res2.images.resize(imagesMap.size());

    // Load images and make add relevant data.
    algo::ImageVectorizer vectorizer{};

    std::unordered_map<std::string, std::tuple<std::vector<glm::vec2>, std::vector<glm::vec2>, std::tuple<glm::vec2, glm::vec2>>> mapped{};

    // Perform operations related on image data first.
    for (const auto &info : imagesMap) {
        int width = 0, height = 0, channels = 0;
        // This gives an 8-bit per channel.
        stbi_uc *imgData = stbi_load((m_assets + info.first).c_str(), &width, &height, &channels, 4); // Force 4 channels (RGBA)

        if (!imgData) {
            return Status::OpenError;
        }

        // Because each pixel is the @var channels values, mult width by @var channels.
        vectorizer.determineImageBorders(algo::MatrixView(imgData, static_cast<size_t>(width * channels), static_cast<size_t>(height)), channels);

        // Now make the Vk Image.
        res2.images[info.second] = engine.createImage(imgData,
                                                      VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                      VK_FORMAT_R8G8B8A8_UNORM,
                                                      VK_IMAGE_USAGE_SAMPLED_BIT);

        stbi_image_free(imgData);

        mapped.insert({info.first, {vectorizer.points, vectorizer.normals, {std::tuple{vectorizer.min, vectorizer.max}}}});
    }

    for (const auto &res : map.resources) {
        const auto &h = res.h, &w = res.w;

        addVertices(w, h, res2);

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

        res2.boundingBoxes.push_back(AB);
        res2.types.push_back(res.type);
        res2.borders.push_back(points);
        res2.normals.push_back(std::get<1>(mapped[res.source]));
    }

    return Status::Ok;
}

void chunkObjectsGrouping(Graphics::Chunk2 &chunk)
{
    const auto groups = group_by<Graphics::ObjectData, &Graphics::ObjectData::animationId>(chunk.objects);
    chunk.references.reserve(groups.size());

    size_t index = 0;
    for (auto group : groups) {
        const size_t start = index;
        const size_t count = std::ranges::distance(group);
        const size_t end = start + count;

        // Safe because the underlying container is contiguous (std::vector) and already sorted.
        chunk.references.push_back({chunk.objects.data() + start, count});

        index = end;
    }
}

Status Map::load2(Graphics::Engine &engine, World::Scene &m_scene)
{
    if (!fs::exists(m_path)) {
        return Status::MissingDirectory;
    }

    if (!fs::is_directory(m_path)) {
        return Status::NotDir;
    }

    const std::string m_assets = m_path + "/assets/";
    if (!fs::exists(m_assets)) {
        return Status::MissingDirectory;
    }

    if (!fs::is_directory(m_assets)) {
        return Status::NotDir;
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
    if (const auto status = readMapFile(paths, names, map)) {
        return status;
    }

    // Now we may need to load from external JSON source file (Resources)
    // Or load from external sources the chunk files.
    if (const auto status = loadResources(paths, names, map)) {
        return status;
    }

    // Check that every image resource does exist.
    if (!std::all_of(map.resources.cbegin(), map.resources.cend(), [&m_assets](const JsonResourceElement &res) {
            return fs::exists(m_assets + res.source);
        })) {
        return Status::MissingResource;
    }

    // Contains ID of the images that are used for animations.
    std::unordered_map<std::string, int> imagesMap{};
    /* Diminish required img count if possible. */ {
        int curr = 0;
        for (const auto &res : map.resources) {
            if (!imagesMap.contains(res.source)) {
                imagesMap.insert({res.source, curr++});
            }
        }
    }

    m_scene.res2 = new Graphics::Resources2{};

    /* Create the animations */ {
        m_scene.res2->animations.reserve(map.resources.size());

        for (const auto &res : map.resources) {
            m_scene.res2->animations.push_back({
                .imageId = static_cast<uint32_t>(imagesMap[res.source]),
                .gridRows = static_cast<uint16_t>(std::get<0>(res.gridSize)),
                .gridColumns = static_cast<uint16_t>(std::get<1>(res.gridSize)),
                .framesCount = static_cast<uint16_t>(res.frames ? res.frames : std::get<0>(res.gridSize) * std::get<1>(res.gridSize)),
                .frameInterval = res.interval,
            });
        }
    }

    if (const auto status = loadChunks(m_path, map)) {
        return status;
    }

    // Now that any external resource have been checked or loaded, we can generate the object.
    // First load the resources.

    const auto resSize = map.resources.size();
    m_scene.res2->images.reserve(resSize);
    m_scene.res2->types.reserve(resSize);
    m_scene.res2->borders.reserve(resSize);
    m_scene.res2->normals.reserve(resSize);

    if (const auto status = buildResources(imagesMap, m_assets, *m_scene.res2, engine, map)) {
        return status;
    }

    m_scene.chunks2.resize(map.chunks.size());
    const auto csize = map.chunks.size();

    m_scene.movings2.objects.resize(map.movings.size());
    for (size_t i = 0; i < csize; ++i) {
        m_scene.chunks2[i].objects.resize(map.chunks[i].size());
    }

    /* Set object IDs */ {
        uint64_t currId = 0;
        const auto fn = [&currId](auto &obj) { obj.objId = currId++; };
        std::for_each(m_scene.movings2.objects.begin(), m_scene.movings2.objects.end(), fn);
        std::for_each(m_scene.chunks2.begin(), m_scene.chunks2.end(), [&currId, fn](auto &chunk) {
            currId = 0;
            std::for_each(chunk.objects.begin(), chunk.objects.end(), fn);
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
            m_scene.movings2.objects[j].animationId = imagesMap[map.resources[map.movings[j].type].source];
        }

        for (size_t i = 0; i < csize; ++i) {
            const auto size = map.chunks[i].size();
            for (size_t j = 0; j < size; ++j) {
                const auto &type = map.chunks[i][j].type;
                const auto &res = map.resources[type];
                const auto v = imagesMap[res.source];
                m_scene.chunks2[i].objects[j].animationId = v;
            }
        }
    }

    /* Set object transforms */ {
        std::for_each(m_scene.movings2.objects.begin(), m_scene.movings2.objects.end(), [](auto &obj) { obj.transform = glm::mat4{1.f}; });
        std::for_each(m_scene.chunks2.begin(), m_scene.chunks2.end(), [](auto &chunk) {
            std::for_each(chunk.objects.begin(), chunk.objects.end(), [](auto &obj) { obj.transform = glm::mat4{1.f}; });
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
        std::for_each(m_scene.movings2.entities.begin(), m_scene.movings2.entities.end(), [&i](auto &e) { e.id = i++; });
        std::for_each(m_scene.chunks2.begin(), m_scene.chunks2.end(), [&i](auto &chunk) {
            std::for_each(chunk.entities.begin(), chunk.entities.end(), [&i](auto &e) { e.id = i++; });
        });
    }

    m_scene.player2 = &m_scene.movings2.objects.front();
    m_scene.res2->build(engine);

    /* Sort elements by their data and build the views */ {
        constexpr auto sort_cmp = [](const auto &a, const auto &b) { return a.animationId < b.animationId; };

        std::sort(m_scene.movings2.objects.begin(), m_scene.movings2.objects.end(), sort_cmp);
        std::for_each(m_scene.chunks2.begin(), m_scene.chunks2.end(), [](auto &chunk) {
            std::sort(chunk.objects.begin(), chunk.objects.end(), [](const auto &a, const auto &b) { return a.animationId < b.animationId; });
        });

        chunkObjectsGrouping(m_scene.movings2);
        std::for_each(m_scene.chunks2.begin(), m_scene.chunks2.end(), &chunkObjectsGrouping);
    }

    m_scene.view2 = m_scene.chunks2 | std::ranges::views::drop(0) | std::ranges::views::take(std::min(size_t(2), csize));

    return Status::Ok;
}
}
