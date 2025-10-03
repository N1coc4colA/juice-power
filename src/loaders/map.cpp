#include "map.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <glaze/glaze.hpp>

#include <magic_enum.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "src/graphics/engine.h"
#include "src/graphics/resources.h"
#include "src/graphics/utils.h"

#include "src/world/chunk.h"
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
    stbi_set_flip_vertically_on_load(false);
}

inline void copyValues(const std::vector<JsonChunkElement> &c, World::Chunk &s, const JsonMap &map, const World::Scene &m_scene)
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
        s.entities[j].mass = map.resources[s.descriptions[j]].mass;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].elasticity = map.resources[s.descriptions[j]].elasticity;
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].borders = m_scene.res->borders[s.descriptions[j]];
    }

    for (size_t j = 0; j < esize; j++) {
        s.entities[j].normals = m_scene.res->normals[s.descriptions[j]];
    }

    for (size_t j = 0; j < esize; j++) {
        const auto &bb = m_scene.res->boundingBoxes[s.descriptions[j]];

        s.entities[j].boundingBox = Physics::AABB{
            .min = std::get<0>(bb),
            .max = std::get<1>(bb),
        };
    }
}

inline void fillBasicObjectInfo(const std::vector<Loaders::JsonChunkElement> &c, World::Chunk &s)
{
    std::transform(c.cbegin(), c.cend(), s.descriptions.begin(), [](const auto &e) { return e.type; });
    std::transform(c.cbegin(), c.cend(), s.positions.begin(), [](const auto &e) { return glm::vec3{e.position[0], e.position[1], e.position[2]}; });
}

inline void prepareVectors(const size_t count, World::Chunk &s)
{
    s.descriptions.resize(count);
    s.positions.resize(count);
    s.animFrames.resize(count);
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

Status Map::load(Graphics::Engine &engine, World::Scene &m_scene)
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
    const auto crop = m_path.length() + 1;

    for (const auto &entry : fs::directory_iterator(m_path)) {
        const auto &p = entry.path();
        paths.push_back(p);
        names.push_back(p.string().substr(crop));
    }

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
		std::cout << "Failed to open file " << paths[pathsMapIndex] << ':' << mapJson.error().location << ':' << magic_enum::enum_name(mapJson.error().ec) << mapJson.error().includer_error << '\n';
		return Status::JsonError;
	}

	JsonMap map = mapJson.value();

	// Now we may need to load from external JSON source file (Resources)
	// Or load from external sources the chunk files.

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

    // Check that every resource does exist.
    if (!std::all_of(map.resources.cbegin(), map.resources.cend(), [&m_assets](const JsonResourceElement &res) {
            return fs::exists(m_assets + res.source);
        })) {
        return Status::MissingResource;
    }

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

    // Now that any external resource have been check or loaded, we can generate the object.
    // First load the resources.

    m_scene.res = new Graphics::Resources{};

    const auto resSize = map.resources.size();
    m_scene.res->images.reserve(resSize);
    m_scene.res->types.reserve(resSize);
    m_scene.res->borders.reserve(resSize);
    m_scene.res->normals.reserve(resSize);

    algo::ImageVectorizer vectorizer{};

    {
        size_t i = 0;
        for (const auto &res : map.resources) {
            // Trya load it.

            int width = 0, height = 0, channels = 0;

            // This gives an 8-bit per channel.
            stbi_uc *imgData = stbi_load((m_assets + res.source).c_str(), &width, &height, &channels, 4); // Force 4 channels (RGBA)

            if (!imgData) {
                return Status::OpenError;
            }

            const auto &h = res.h;
            const auto &w = res.w;
            const Graphics::Resources::Vertices vertices{.data = {
                                                             {
                                                                 .position = {0.f, 0.f, 0.f},
                                                                 .uv_x = 0.f,
                                                                 .normal = {0.f, 0.f, 1.f},
                                                                 .uv_y = 0.f,
                                                                 .color = glm::vec4(0.f, 1.f, 0.f, 1.f),
                                                             },
                                                             {
                                                                 .position = {0.f, h, 0.f},
                                                                 .uv_x = 0.f,
                                                                 .normal = {0.f, 0.f, 1.f},
                                                                 .uv_y = 1.f,
                                                                 .color = glm::vec4(1.f, 0.f, 0.f, 1.f),
                                                             },
                                                             {
                                                                 .position = {w, 0.f, 0.f},
                                                                 .uv_x = 1.f,
                                                                 .normal = {0.f, 0.f, 1.f},
                                                                 .uv_y = 0.f,
                                                                 .color = glm::vec4(0.f, 0.f, 1.f, 1.f),
                                                             },
                                                             {
                                                                 .position = {w, h, 0.f},
                                                                 .uv_x = 1.f,
                                                                 .normal = {0.f, 0.f, 1.f},
                                                                 .uv_y = 1.f,
                                                                 .color = glm::vec4(1.f, 1.f, 0.f, 1.f),
                                                             },
                                                         }};

            // Because each pixel is the @var channels values, mult width by @var channels.
            vectorizer.determineImageBorders(algo::MatrixView(imgData, static_cast<size_t>(width * channels), static_cast<size_t>(height)), channels);

            for (auto &p : vectorizer.points) {
                p.x *= w;
            }
            for (auto &p : vectorizer.points) {
                p.y *= h;
            }

            m_scene.res->boundingBoxes.push_back(std::tuple{vectorizer.min, vectorizer.max});

            m_scene.res->types.push_back(res.type);
            m_scene.res->vertices.push_back(vertices);
            m_scene.res->borders.push_back(vectorizer.points);
            m_scene.res->normals.push_back(vectorizer.normals);

            // Now make the Vk Image.
            m_scene.res->images.push_back(engine.createImage(imgData,
                                                             VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                                                             VK_FORMAT_R8G8B8A8_UNORM,
                                                             VK_IMAGE_USAGE_SAMPLED_BIT));

            stbi_image_free(imgData);

            m_scene.res->animColumns.push_back(res.gridSize[0]);
            m_scene.res->animRows.push_back(res.gridSize[1]);
            m_scene.res->animInterval.push_back(res.interval);
            m_scene.res->animFrames.push_back(res.frames ? res.frames : res.gridSize[0] * res.gridSize[1]);

            ++i;
        }
    }

    m_scene.chunks.resize(map.chunks.size());
    const auto csize = map.chunks.size();

    // Setup transform matrices
    m_scene.movings.transforms = std::vector(map.movings.size(), glm::mat4{1.f});
    for (size_t i = 0; i < csize; ++i) {
        // At first, all objects have their usual transform matrix.
        m_scene.chunks[i].transforms = std::vector(map.chunks[i].size(), glm::mat4{1.f});
    }

    // We fill it in later to avoid constructing and then change the data.
    prepareVectors(map.movings.size(), m_scene.movings);
    for (size_t i = 0; i < csize; ++i) {
        prepareVectors(map.chunks[i].size(), m_scene.chunks[i]);
    }

    // Fill in basic data
    fillBasicObjectInfo(map.movings, m_scene.movings);
    for (size_t i = 0; i < csize; ++i) {
        fillBasicObjectInfo(map.chunks[i], m_scene.chunks[i]);
    }

    // Update entities' information.
    copyValues(map.movings, m_scene.movings, map, m_scene);
    for (size_t i = 0; i < csize; ++i) {
        copyValues(map.chunks[i], m_scene.chunks[i], map, m_scene);
    }

    {
        size_t i = 0;
        for (auto &e : m_scene.movings.entities) {
            e.id = i++;
        }
        for (auto &c : m_scene.chunks) {
            for (auto &e : c.entities) {
                e.id = i++;
            }
        }
    }

    m_scene.res->build(engine);

    m_scene.view = m_scene.chunks | std::ranges::views::drop(0) | std::ranges::views::take(std::min(size_t(2), csize));

    return Status::Ok;
}


}
